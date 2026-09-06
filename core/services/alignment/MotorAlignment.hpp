#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/Function.hpp"
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
        };

        virtual void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) = 0;

        // Stops the alignment voltage and drops the pending completion without invoking it: the
        // caller that aborts owns the outcome, and a fault must not be overwritten by a late result.
        virtual void Abort() = 0;
    };
}
