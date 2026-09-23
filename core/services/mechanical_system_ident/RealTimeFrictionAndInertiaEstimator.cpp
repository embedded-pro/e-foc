#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <cmath>
#include <optional>

namespace services
{
    namespace
    {
        // The online regression runs in milli-newton-metres against micro-unit inertia and friction, so the three
        // columns are of order one on the rotors this drive is sized for and float keeps its precision
        constexpr float outputScale{ 1e3f };
        constexpr float parameterScale{ 1e6f };
        constexpr float columnScale{ outputScale / parameterScale };

        constexpr float onlineInitialCovariance{ 1e4f };
        constexpr float onlineCovarianceCeiling{ 1e5f };
        constexpr uint16_t onlineMinimumUpdates{ 1000 };
        constexpr float residualAveragingWeight{ 1e-3f };
        constexpr float settledResidualRatio{ 0.1f };
    }

    RealTimeFrictionAndInertiaEstimator::RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency)
        : samplingFrequency(static_cast<float>(samplingFrequency.Value()))
        , forgettingFactor(forgettingFactor)
        , rls(std::in_place, 1000.0f, forgettingFactor)
        , onlineRls(std::in_place, onlineInitialCovariance, forgettingFactor)
    {
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
        onlineRls.emplace(onlineInitialCovariance, forgettingFactor);

        MotorRLS::CoefficientsMatrix initial{};
        initial.at(1, 0) = currentInertia.Value() * parameterScale;
        initial.at(2, 0) = currentFriction.Value() * parameterScale;
        onlineRls->SetCoefficients(initial);

        onlineMetrics = MotorRLS::EstimationMetrics{};
        onlinePrimed = false;
        onlineUpdates = 0;
        onlineExcitation.Restart();
        residualPower = 0.0f;
        outputPower = 0.0f;
    }

    // Window averages obey the momentum balance exactly when the torque is averaged over the same span the
    // speed difference covers: J (w_k - w_k-1) / T = kt (i_k + i_k-1) / 2 - B (w_k + w_k-1) / 2 - tau
    void RealTimeFrictionAndInertiaEstimator::Update(const foc::MechanicalWindow& window)
    {
        const auto speed = window.meanSpeed.Value();
        const auto iq = window.meanIq.Value();

        if (!onlinePrimed)
        {
            previousWindowSpeed = speed;
            previousWindowIq = iq;
            onlinePrimed = true;
            return;
        }

        const auto acceleration = (speed - previousWindowSpeed) * samplingFrequency;
        const auto midSpeed = 0.5f * (speed + previousWindowSpeed);
        const auto midIq = 0.5f * (iq + previousWindowIq);
        previousWindowSpeed = speed;
        previousWindowIq = iq;

        if (!IsOnlineObservationInformative(acceleration, midSpeed))
            return;

        MotorRLS::MakeRegressor(onlineRegressor, acceleration * columnScale, midSpeed * columnScale);
        math::Matrix<float, 1, 1> output;
        output.at(0, 0) = torqueConstant.Value() * midIq * outputScale;

        onlineMetrics = onlineRls->Update(onlineRegressor, output);
        residualPower += residualAveragingWeight * (onlineMetrics.residual * onlineMetrics.residual - residualPower);
        outputPower += residualAveragingWeight * (output.at(0, 0) * output.at(0, 0) - outputPower);

        onlineExcitation.Count(acceleration, midSpeed);
        if (onlineUpdates != onlineMinimumUpdates)
            ++onlineUpdates;

        PublishOnlineEstimate();
    }

    // A plateau still separates friction from the intercept, but says nothing about inertia; letting it in
    // unconditionally would inflate that direction of the covariance without bound on a long constant-speed run
    bool RealTimeFrictionAndInertiaEstimator::IsOnlineObservationInformative(float acceleration, float speed) const
    {
        if (!IsMechanicallyObservable(speed))
            return false;

        return std::abs(acceleration) >= mechanical_estimate::minimumAcceleration || onlineMetrics.uncertainty <= onlineCovarianceCeiling;
    }

    // Publishing an estimate turns it into speed-loop gains, so it waits for a fit that has seen acceleration
    // (inertia), more than one speed (friction apart from the intercept), and whose residual is a small
    // fraction of the torque it explains
    bool RealTimeFrictionAndInertiaEstimator::HasOnlineEstimateSettled() const
    {
        return onlineUpdates >= onlineMinimumUpdates &&
               onlineExcitation.AcceleratingObservations() >= mechanical_estimate::minimumExcitedUpdates &&
               onlineExcitation.HasSpannedDistinctSpeeds() &&
               residualPower <= settledResidualRatio * settledResidualRatio * outputPower;
    }

    void RealTimeFrictionAndInertiaEstimator::PublishOnlineEstimate()
    {
        if (!HasOnlineEstimateSettled())
            return;

        const auto inertia = onlineRls->Coefficients().at(1, 0) / parameterScale;
        const auto friction = onlineRls->Coefficients().at(2, 0) / parameterScale;

        if (!IsPlausibleMechanics(inertia, friction))
            return;

        currentInertia = foc::NewtonMeterSecondSquared{ inertia };
        currentFriction = foc::NewtonMeterSecondPerRadian{ friction };
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
