#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include <gmock/gmock.h>

namespace integration
{
    class PlatformFactoryMock
        : public application::PlatformFactory
    {
    public:
        MOCK_METHOD(void, Run, (), (override));
        MOCK_METHOD(services::Tracer&, Tracer, (), (override));
        MOCK_METHOD(services::TerminalWithCommands&, Terminal, (), (override));
        MOCK_METHOD(infra::MemoryRange<hal::GpioPin>, Leds, (), (override));
        MOCK_METHOD(hal::PerformanceTracker&, PerformanceTimer, (), (override));
        MOCK_METHOD(hal::Hertz, SystemClock, (), (const, override));
        MOCK_METHOD(foc::Volts, PowerSupplyVoltage, (), (override));
        MOCK_METHOD(foc::Ampere, MaxCurrentSupported, (), (override));
        MOCK_METHOD(foc::LowPriorityInterrupt&, LowPriorityInterrupt, (), (override));
        MOCK_METHOD(
            (infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>&),
            SynchronousThreeChannelsPwmCreator, (), (override));
        MOCK_METHOD(
            (infra::CreatorBase<application::AdcPhaseCurrentMeasurement, void(SampleAndHold)>&),
            AdcMultiChannelCreator, (), (override));
        MOCK_METHOD(
            (infra::CreatorBase<application::QuadratureEncoderDecorator, void()>&),
            SynchronousQuadratureEncoderCreator, (), (override));
        MOCK_METHOD(
            (infra::CreatorBase<application::CanBusAdapter, void(uint32_t bitRate, bool testMode)>&),
            CanBusCreator, (), (override));
        MOCK_METHOD(hal::Eeprom&, Eeprom, (), (override));
        MOCK_METHOD(void, Reset, (), (override));
        MOCK_METHOD(application::ResetCause, GetResetCause, (), (const, override));
        MOCK_METHOD(infra::BoundedConstString, FaultStatus, (), (const, override));
    };
}
