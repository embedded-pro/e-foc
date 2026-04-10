#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/Function.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <optional>

namespace services
{
    class MotorAlignment
    {
    public:
        struct AlignmentConfig
        {
            hal::Percent testVoltagePercent{ 20 };
            hal::Hertz samplingFrequency{ 1000 };
            std::size_t maxSamples{ 500 };
            foc::Radians settledThreshold{ 0.001f };
            std::size_t settledCount{ 10 };
        };

        virtual void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) = 0;
    };
}
