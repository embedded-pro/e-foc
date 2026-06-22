#pragma once

#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/test_doubles/CanBusAdapterMock.hpp"
#include <gmock/gmock.h>

namespace application
{
    class PlatformFactoryMock
        : public PlatformFactory
    {
    public:
        MOCK_METHOD(void, Run, (), (override));
        MOCK_METHOD(services::Tracer&, Tracer, (), (override));
        MOCK_METHOD(services::TerminalWithCommands&, Terminal, (), (override));
        MOCK_METHOD(infra::MemoryRange<hal::GpioPin>, Leds, (), (override));
        MOCK_METHOD(hal::PerformanceTracker&, PerformanceTimer, (), (override));
        MOCK_METHOD(hal::Hertz, SystemClock, (), (const, override));
        MOCK_METHOD(foc::Volts, PowerSupplyVoltage, (), (override));
        MOCK_METHOD(foc::LowPriorityInterrupt&, LowPriorityInterrupt, (), (override));
        MOCK_METHOD(hal::Eeprom&, Eeprom, (), (override));
        MOCK_METHOD(void, Reset, (), (override));
        MOCK_METHOD(ResetCause, GetResetCause, (), (const, override));
        MOCK_METHOD(infra::BoundedConstString, FaultStatus, (), (const, override));

        MOCK_METHOD(void, ConfigureAdcAndPwm, (hal::Hertz, std::chrono::nanoseconds, SampleAndHold), (override));
        MOCK_METHOD(void, SetEncoderResolution, (uint32_t), (override));
        MOCK_METHOD(void, ConfigureCanBus, (uint32_t, bool), (override));
        MOCK_METHOD(CanBusAdapter&, CanBus, (), (override));

        MOCK_METHOD(void, RegisterBoardProtection, (const infra::Function<void(BoardProtectionReason)>&), (override));

        // ThreePhaseInverter
        MOCK_METHOD(void, PhaseCurrentsReady, (hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>&), (override));
        MOCK_METHOD(void, ThreePhasePwmOutput, (const foc::PhasePwmDutyCycles&), (override));
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(hal::Hertz, BaseFrequency, (), (const, override));
        MOCK_METHOD(foc::Ampere, MaxCurrentSupported, (), (const, override));

        // Encoder
        MOCK_METHOD(foc::Radians, Read, (), (override));
        MOCK_METHOD(void, Set, (foc::Radians), (override));
        MOCK_METHOD(void, SetZero, (), (override));
    };
}
