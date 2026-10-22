#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/cogging_compensation/CoggingMap.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstddef>
#include <optional>

namespace services
{
    class CoggingMapping
    {
    public:
        virtual ~CoggingMapping() = default;

        struct Config
        {
            foc::RadiansPerSecond speed{ 10.0f };
            float settleTurns{ 0.5f };
            float measuredTurns{ 2.0f };
            float speedSlewRate{ 200.0f };
            foc::Ampere maxCurrent{ 10.0f };
            infra::Duration timeout{ std::chrono::seconds{ 10 } };
        };

        virtual void Measure(foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, std::size_t polePairs, const Config& config, const infra::Function<void(std::optional<CoggingMap>)>& onDone) = 0;
        virtual bool IsRunning() const = 0;
        virtual void Abort() = 0;
    };
}
