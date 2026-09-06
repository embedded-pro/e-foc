#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <optional>

namespace services
{
    class MotorAlignment
    {
    public:
        virtual ~MotorAlignment() = default;

        struct AlignmentConfig
        {
            hal::Percent testVoltagePercent{ 20 };
            hal::Hertz samplingFrequency{ 1000 };
            std::size_t maxSamples{ 500 };
            foc::Radians settledThreshold{ 0.001f };
            std::size_t settledCount{ 10 };
            // maxSamples bounds the run only while samples keep arriving. Nothing else does, so a
            // drive that never starts leaves the caller waiting on a completion that never comes.
            infra::Duration timeout{ std::chrono::seconds{ 2 } };
        };

        virtual void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) = 0;
    };
}
