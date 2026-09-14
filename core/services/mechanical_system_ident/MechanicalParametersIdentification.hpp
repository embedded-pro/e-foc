#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <optional>

namespace services
{
    class MechanicalParametersIdentification
    {
    public:
        virtual ~MechanicalParametersIdentification() = default;

        struct IdentificationResult
        {
            foc::NewtonMeterSecondPerRadian friction;
            foc::NewtonMeterSecondSquared inertia;
        };

        // targetSpeed and dwellSpeed are the two levels of the excitation trajectory; the run alternates
        // between them every dwellTime so that each transition excites the acceleration column of the
        // regressor. maxCurrent and maxSpeed bound the motion the trajectory is allowed to produce, and
        // timeout bounds its duration. The forgetting factor is near unity because the plant does not drift
        // within one short run, and a shorter memory leaves the steady-state covariance above the
        // convergence threshold this run must reach.
        struct Config
        {
            foc::RadiansPerSecond targetSpeed{ 52.36f };
            float forgettingFactor{ 0.9995f };
            infra::Duration timeout{ std::chrono::seconds{ 5 } };
            foc::RadiansPerSecond dwellSpeed{ 26.18f };
            infra::Duration dwellTime{ std::chrono::milliseconds{ 250 } };
            foc::Ampere maxCurrent{ 10.0f };
            foc::RadiansPerSecond maxSpeed{ 104.72f };
        };

        virtual void EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone) = 0;
        virtual bool IsRunning() const = 0;

        virtual void Abort() = 0;
    };

    inline bool IsUsableIdentificationConfig(const MechanicalParametersIdentification::Config& config)
    {
        return foc::IsFinitePositive(config.targetSpeed.Value()) &&
               foc::IsFinitePositive(config.dwellSpeed.Value()) &&
               config.dwellSpeed.Value() < config.targetSpeed.Value() &&
               foc::IsFinitePositive(config.maxCurrent.Value()) &&
               config.maxSpeed.Value() >= config.targetSpeed.Value() &&
               foc::IsWithinInclusive(config.forgettingFactor, 0.0f, 1.0f) && config.forgettingFactor > 0.0f &&
               config.dwellTime > infra::Duration::zero() &&
               config.timeout > config.dwellTime;
    }
}
