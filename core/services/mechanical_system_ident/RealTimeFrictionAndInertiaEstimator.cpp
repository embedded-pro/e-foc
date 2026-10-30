#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/foc/interfaces/Units.hpp"
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
        , kinematics(1.0f / this->samplingFrequency)
    {
        ReseedOnline();
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

        // Friction comes from the constant-speed runs; the full fit's viscous term takes transients too.
        const auto inertia = fit.rls->Coefficients().at(1, 0) / parameterScale;
        const auto steadyFriction = SteadyFriction();
        const auto friction = steadyFriction.has_value() ? SettledFriction(*steadyFriction / parameterScale) : currentFriction.Value();

        if (!IsPlausibleMechanics(inertia, friction))
            return;

        currentInertia = foc::NewtonMeterSecondSquared{ inertia };
        currentFriction = foc::NewtonMeterSecondPerRadian{ friction };
    }

    // Viscous friction is the same both ways, so the slope is pooled over both directions' runs.
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
