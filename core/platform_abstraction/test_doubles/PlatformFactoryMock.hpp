#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include <gmock/gmock.h>

namespace application
{
    class PlatformFactoryMock
        : public PlatformFactory
    {
    public:
        PlatformFactoryMock()
        {
            ON_CALL(*this, MaxCurrentSupported()).WillByDefault(testing::Return(foc::Ampere{ defaultMaxCurrent }));
            EXPECT_CALL(*this, MaxCurrentSupported()).Times(testing::AnyNumber());
        }

        static constexpr float defaultMaxCurrent{ 10.0f };

        MOCK_METHOD(void, Run, (), (override));
        MOCK_METHOD(services::Tracer&, Tracer, (), (override));
        MOCK_METHOD(services::TerminalWithCommands&, Terminal, (), (override));
        MOCK_METHOD(hal::GpioPin&, OperationalLed, (), (override));
        MOCK_METHOD(hal::GpioPin&, WarningLed, (), (override));
        MOCK_METHOD(hal::GpioPin&, FailureLed, (), (override));
        MOCK_METHOD(uint8_t, BoardId, (), (const, override));
        MOCK_METHOD(bool, PowerStatus, (), (const, override));
        MOCK_METHOD(hal::PerformanceTracker&, PerformanceTimer, (), (override));
        MOCK_METHOD(hal::Hertz, SystemClock, (), (const, override));
        MOCK_METHOD(foc::Volts, PowerSupplyVoltage, (), (override));
        MOCK_METHOD(foc::LowPriorityInterrupt&, LowPriorityInterrupt, (), (override));
        MOCK_METHOD(hal::Eeprom&, Eeprom, (), (override));
        MOCK_METHOD(application::ControlLoopMetrics::Snapshot, ControlLoopStatistics, (), (const, override));
        MOCK_METHOD(const application::CanBusAdapter::ErrorCounters&, CanStatistics, (), (const, override));
        MOCK_METHOD(void, ResetStatistics, (), (override));
        MOCK_METHOD(void, Reset, (), (override));
        MOCK_METHOD(application::ResetCause, GetResetCause, (), (const, override));
        MOCK_METHOD(infra::BoundedConstString, FaultStatus, (), (const, override));
        MOCK_METHOD(void, RegisterBoardProtection, (const infra::Function<void(PlatformFactory::BoardProtectionReason)>&), (override));

        MOCK_METHOD(void, ConfigureAdcAndPwm, (hal::Hertz, std::chrono::nanoseconds, SampleAndHold), (override));
        MOCK_METHOD(void, SetEncoderResolution, (uint32_t), (override));
        MOCK_METHOD(void, ConfigureCanBus, (uint32_t, bool), (override));
        MOCK_METHOD(application::CanBusAdapter&, CanBus, (), (override));

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

        void StoreBoardProtectionHandler(const infra::Function<void(BoardProtectionReason)>& handler)
        {
            storedHandler = handler;
        }

        void RaiseBoardProtection(BoardProtectionReason reason)
        {
            if (storedHandler != nullptr)
                storedHandler(reason);
        }

    private:
        infra::Function<void(BoardProtectionReason)> storedHandler;
    };
}
