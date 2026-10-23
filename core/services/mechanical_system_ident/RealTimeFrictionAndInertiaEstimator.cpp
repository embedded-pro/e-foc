#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace services
{
    namespace
    {
        constexpr float outputScale{ 1e3f };
        constexpr float parameterScale{ 1e6f };
        constexpr float columnScale{ outputScale / parameterScale };

        constexpr float onlineInitialCovariance{ 1e4f };
        constexpr float onlineCovarianceCeiling{ 1e5f };
        constexpr uint16_t onlineMinimumPeriods{ 1000 };
        constexpr float residualAveragingWeight{ 1e-3f };
        constexpr float settledResidualRatio{ 0.25f };
    }

    RealTimeFrictionAndInertiaEstimator::RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency)
        : samplingFrequency(static_cast<float>(samplingFrequency.Value()))
        , forgettingFactor(forgettingFactor)
        , rls(std::in_place, 1000.0f, forgettingFactor)
        , kinematics(1.0f / this->samplingFrequency)
    {
        ReseedOnline();
    }

    RealTimeFrictionAndInertiaEstimator::Result RealTimeFrictionAndInertiaEstimator::Update(foc::PhaseCurrents currentPhases, foc::RadiansPerSecond speed, foc::Radians electricalAngle, foc::NewtonMeter targetTorque)
    {
        auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle.Value()), foc::FastTrigonometry::Sine(electricalAngle.Value()));
        auto acceleration = (speed.Value() - previousSpeed.Value()) * samplingFrequency;

        previousSpeed = speed;

        if (!IsMechanicallyObservable(speed.Value()))
            return Result{
                foc::NewtonMeterSecondSquared{ rls->Coefficients().at(1, 0) },
                foc::NewtonMeterSecondPerRadian{ rls->Coefficients().at(2, 0) },
                lastMetrics
            };

        MotorRLS::MakeRegressor(regressor, acceleration, speed.Value());

        torque.at(0, 0) = rotatingFrame.q * targetTorque.Value();

        lastMetrics = rls->Update(regressor, torque);
        excitation.Count(acceleration, speed.Value());

        return Result{
            foc::NewtonMeterSecondSquared{ rls->Coefficients().at(1, 0) },
            foc::NewtonMeterSecondPerRadian{ rls->Coefficients().at(2, 0) },
            lastMetrics
        };
    }

    void RealTimeFrictionAndInertiaEstimator::Seed(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction)
    {
        if (!rls.has_value())
            return;

        // RLS model: torque = theta[0]*1(coulomb) + theta[1]*acceleration(inertia) + theta[2]*speed(friction)
        MotorRLS::CoefficientsMatrix initial{};
        initial.at(0, 0) = 0.0f;
        initial.at(1, 0) = inertia.Value();
        initial.at(2, 0) = friction.Value();
        rls->SetCoefficients(initial);
        lastMetrics = MotorRLS::EstimationMetrics{};
        excitation.Restart();
    }

    void RealTimeFrictionAndInertiaEstimator::SetTorqueConstant(foc::NewtonMeter kt)
    {
        torqueConstant = kt;
    }

    void RealTimeFrictionAndInertiaEstimator::SetInitialEstimate(
        foc::NewtonMeterSecondSquared inertia,
        foc::NewtonMeterSecondPerRadian friction)
    {
        currentInertia = inertia;
        currentFriction = friction;
        Seed(inertia, friction);
        ReseedOnline();
    }

    void RealTimeFrictionAndInertiaEstimator::ReseedOnline()
    {
        MotorRLS::CoefficientsMatrix initial{};
        initial.at(1, 0) = currentInertia.Value() * parameterScale;
        initial.at(2, 0) = currentFriction.Value() * parameterScale;

        for (auto& fit : onlineFits)
        {
            fit = DirectionalFit{};
            fit.rls.emplace(onlineInitialCovariance, forgettingFactor);
            fit.rls->SetCoefficients(initial);
        }

        kinematics.Restart();
    }

    void RealTimeFrictionAndInertiaEstimator::Update(const foc::MechanicalWindow& window)
    {
        const auto observation = kinematics.Add(window);
        if (!observation.has_value())
            return;

        if (!IsOnlineObservationInDirection(observation->acceleration, observation->speed))
        {
            for (auto& directional : onlineFits)
                directional.steady.EndRun();
            return;
        }

        auto& fit = onlineFits[observation->speed >= 0.0f ? 0 : 1];
        math::Matrix<float, 1, 1> output;
        output.at(0, 0) = torqueConstant.Value() * observation->iq * outputScale;
        fit.steady.Add(observation->speed * columnScale, output.at(0, 0) - fit.rls->Coefficients().at(1, 0) * observation->acceleration * columnScale, forgettingFactor);

        if (!IsOnlineObservationInformative(observation->acceleration, fit))
            return;

        MotorRLS::MakeRegressor(onlineRegressor, observation->acceleration * columnScale, observation->speed * columnScale);

        fit.metrics = fit.rls->Update(onlineRegressor, output);
        fit.residualPower += residualAveragingWeight * (fit.metrics.residual * fit.metrics.residual - fit.residualPower);
        fit.outputPower += residualAveragingWeight * (output.at(0, 0) * output.at(0, 0) - fit.outputPower);

        fit.excitation.CountAcceleration(observation->acceleration);
        if (std::abs(observation->acceleration) < mechanical_estimate::minimumAcceleration)
            fit.excitation.CountSpeed(observation->speed);
        fit.periods = static_cast<uint16_t>(std::min<uint32_t>(fit.periods + kinematics.WindowsPerObservation(), onlineMinimumPeriods));

        PublishOnlineEstimate(fit);
    }

    bool RealTimeFrictionAndInertiaEstimator::IsOnlineObservationInDirection(float acceleration, float speed) const
    {
        return IsMechanicallyObservable(speed) && std::abs(speed) > std::abs(acceleration) * static_cast<float>(kinematics.WindowsPerObservation()) / samplingFrequency;
    }

    bool RealTimeFrictionAndInertiaEstimator::IsOnlineObservationInformative(float acceleration, const DirectionalFit& fit) const
    {
        return std::abs(acceleration) >= mechanical_estimate::minimumAcceleration || fit.metrics.uncertainty <= onlineCovarianceCeiling;
    }

    bool RealTimeFrictionAndInertiaEstimator::HasOnlineEstimateSettled(const DirectionalFit& fit) const
    {
        return fit.periods >= onlineMinimumPeriods &&
               fit.excitation.AcceleratingObservations() * kinematics.WindowsPerObservation() >= mechanical_estimate::minimumExcitedUpdates &&
               fit.excitation.HasSpannedDistinctSpeeds() &&
               fit.residualPower <= settledResidualRatio * settledResidualRatio * fit.outputPower;
    }

    void RealTimeFrictionAndInertiaEstimator::PublishOnlineEstimate(const DirectionalFit& fit)
    {
        if (!HasOnlineEstimateSettled(fit))
            return;

        // The full fit's viscous term also takes the torque of every transient and every ripple, which on the rig
        // outweighed the small difference between constant speeds; the runs at constant speed give the friction
        const auto inertia = fit.rls->Coefficients().at(1, 0) / parameterScale;
        const auto steadyFriction = SteadyFriction();
        const auto friction = steadyFriction.has_value() ? SettledFriction(*steadyFriction / parameterScale) : currentFriction.Value();

        if (!IsPlausibleMechanics(inertia, friction))
            return;

        currentInertia = foc::NewtonMeterSecondSquared{ inertia };
        currentFriction = foc::NewtonMeterSecondPerRadian{ friction };
    }

    // The viscous friction is the same both ways while the intercepts are not, so the slope is pooled over both
    // directions' runs, each about its own means: a direction that has only held speeds where the friction is still
    // flat, as the rig's is from 10 to 30 rad/s, cannot set it alone, and the last direction run does not choose it
    std::optional<float> RealTimeFrictionAndInertiaEstimator::SteadyFriction() const
    {
        auto variance = 0.0f;
        auto covariance = 0.0f;
        auto spansDistinctSpeeds = false;

        for (const auto& fit : onlineFits)
        {
            variance += fit.steady.SpeedVariance();
            covariance += fit.steady.SpeedTorqueCovariance();
            spansDistinctSpeeds = spansDistinctSpeeds || fit.steady.SpansDistinctSpeeds();
        }

        if (!spansDistinctSpeeds)
            return std::nullopt;

        return covariance / variance;
    }

    foc::NewtonMeterSecondSquared RealTimeFrictionAndInertiaEstimator::CurrentInertia() const
    {
        return currentInertia;
    }

    foc::NewtonMeterSecondPerRadian RealTimeFrictionAndInertiaEstimator::CurrentFriction() const
    {
        return currentFriction;
    }
}
