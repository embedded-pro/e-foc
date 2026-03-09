#pragma once

#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "source/foc/interfaces/Units.hpp"
#include <chrono>
#include <optional>

namespace services
{
    class MechanicalParametersIdentification
    {
    public:
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
    };
}
