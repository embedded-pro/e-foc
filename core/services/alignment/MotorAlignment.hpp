#pragma once

#include "e-foc-protocol/foc/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace services
{
    class MotorAlignment
    {
    protected:
        ~MotorAlignment() = default;

    public:
        struct AlignmentConfig
        {
            hal::DutyCycle testVoltage{ hal::DutyCycle::FromPercent(20) };
            hal::Hertz samplingFrequency{ 1000 };
            std::size_t maxSamples{ 500 };
            foc::Radians settledThreshold{ 0.001f };
            std::size_t settledCount{ 10 };
            infra::Duration timeout{ std::chrono::seconds{ 2 } };
            foc::Ohm phaseResistance{ 0.0f };
            foc::Henry phaseInductance{ 0.0f };
            foc::Volts busVoltage{ 0.0f };
            std::optional<foc::Radians> followStep;
            std::size_t followDwell{ 100 };
        };

        enum class Failure : uint8_t
        {
            none,
            phaseMissing,
            encoderNotFollowing
        };

        virtual void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) = 0;

        virtual void Abort() = 0;

        virtual Failure LastFailure() const = 0;
    };
}
