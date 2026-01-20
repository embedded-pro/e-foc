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
        struct FrictionConfig
        {
            foc::RadiansPerSecond targetSpeed{ 52.36f };
            infra::Duration settleTime{ std::chrono::seconds{ 3 } };
            infra::Duration measurementTime{ std::chrono::seconds{ 2 } };
            foc::NewtonMeter torqueConstant{ 0.1f };
        };

        struct InertiaConfig
        {
            foc::Ampere torqueStepCurrent{ 1.0f };
            infra::Duration accelerationTime{ std::chrono::milliseconds{ 500 } };
            foc::NewtonMeter torqueConstant{ 0.1f };
        };

        virtual void EstimateFriction(const FrictionConfig& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)>& onDone) = 0;
        virtual void EstimateInertia(const InertiaConfig& config, foc::NewtonMeterSecondPerRadian damping, const infra::Function<void(std::optional<foc::NewtonMeterSecondSquared>)>& onDone) = 0;
    };
}
