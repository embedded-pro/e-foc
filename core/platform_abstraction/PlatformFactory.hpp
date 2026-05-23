#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <chrono>
#include <cstdint>

namespace hal
{
    class PerformanceTracker
    {
    public:
        virtual void Start() = 0;
        virtual uint32_t ElapsedCycles() = 0;
    };
}

namespace application
{
    enum class ResetCause : uint8_t
    {
        powerUp = 0,
        brownOut = 1,
        software = 2,
        hardware = 3,
        watchdog = 4,
    };

    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, ResetCause cause);

    class PlatformFactory
        : public foc::ThreePhaseInverter
        , public foc::Encoder
    {
    public:
        enum class SampleAndHold
        {
            shortest,
            shorter,
            medium,
            longer,
            longest,
        };

        enum class BoardProtectionReason : uint8_t
        {
            overCurrent,
            overVoltage,
            overTemperature,
        };

        virtual void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) = 0;
        virtual void SetEncoderResolution(uint32_t resolution) = 0;
        virtual void ConfigureCanBus(uint32_t bitRate, bool testMode) = 0;
        virtual CanBusAdapter& CanBus() = 0;

        virtual void Run() = 0;
        virtual services::Tracer& Tracer() = 0;
        virtual services::TerminalWithCommands& Terminal() = 0;
        virtual infra::MemoryRange<hal::GpioPin> Leds() = 0;
        virtual hal::PerformanceTracker& PerformanceTimer() = 0;
        virtual hal::Hertz SystemClock() const = 0;
        virtual foc::Volts PowerSupplyVoltage() = 0;
        virtual foc::LowPriorityInterrupt& LowPriorityInterrupt() = 0;
        virtual hal::Eeprom& Eeprom() = 0;

        virtual void RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection) = 0;

        virtual void Reset() = 0;
        virtual ResetCause GetResetCause() const = 0;
        virtual infra::BoundedConstString FaultStatus() const = 0;
    };
}
