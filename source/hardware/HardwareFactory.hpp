#pragma once

#include "hal/interfaces/Gpio.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ProxyCreator.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/hardware/AdcPhaseCurrentMeasurement.hpp"
#include "source/hardware/CanBusAdapter.hpp"
#include "source/hardware/QuadratureEncoderDecorator.hpp"
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
    class HardwareFactory
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

        virtual void Run() = 0;
        virtual services::Tracer& Tracer() = 0;
        virtual services::TerminalWithCommands& Terminal() = 0;
        virtual infra::MemoryRange<hal::GpioPin> Leds() = 0;
        virtual hal::PerformanceTracker& PerformanceTimer() = 0;
        virtual hal::Hertz SystemClock() const = 0;
        virtual foc::Volts PowerSupplyVoltage() = 0;
        virtual foc::Ampere MaxCurrentSupported() = 0;
        virtual foc::LowPriorityInterrupt& LowPriorityInterrupt() = 0;
        virtual infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>& SynchronousThreeChannelsPwmCreator() = 0;
        virtual infra::CreatorBase<AdcPhaseCurrentMeasurement, void(SampleAndHold)>& AdcMultiChannelCreator() = 0;
        virtual infra::CreatorBase<QuadratureEncoderDecorator, void()>& SynchronousQuadratureEncoderCreator() = 0;
        virtual infra::CreatorBase<CanBusAdapter, void(uint32_t bitRate, bool testMode)>& CanBusCreator() = 0;
    };
}
