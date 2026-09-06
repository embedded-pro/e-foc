#pragma once

#include "core/foc/interfaces/Units.hpp"
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

        struct Config
        {
            foc::RadiansPerSecond targetSpeed{ 52.36f };
            float forgettingFactor{ 0.998f };
            infra::Duration timeout{ std::chrono::seconds{ 5 } };
        };

        virtual void EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone) = 0;

        // Releases the drive and drops the pending completion without invoking it: the caller that
        // aborts owns the outcome, and a fault must not be overwritten by a late calibration result.
        virtual void Abort() = 0;
    };
}
