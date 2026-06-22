#include "core/platform_abstraction/test_doubles/PlatformFactoryMock.hpp"
#include "core/state_machine/BoardProtectionFaultNotifier.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    class TestBoardProtectionFaultNotifier
        : public ::testing::Test
    {
    protected:
        void GivenNotifierConstructed()
        {
            EXPECT_CALL(platformMock, RegisterBoardProtection(_))
                .WillOnce(Invoke([this](const infra::Function<void(application::PlatformFactory::BoardProtectionReason)>& cb)
                    {
                        capturedProtectionCallback = cb;
                    }));

            notifier.emplace(platformMock);
        }

        void TriggerProtection(application::PlatformFactory::BoardProtectionReason reason)
        {
            capturedProtectionCallback(reason);
        }

        StrictMock<application::PlatformFactoryMock> platformMock;
        infra::Function<void(application::PlatformFactory::BoardProtectionReason)> capturedProtectionCallback;
        std::optional<state_machine::BoardProtectionFaultNotifier> notifier;

        std::optional<state_machine::FaultCode> receivedStateMachineCode;
        std::optional<state_machine::FaultCode> receivedBroadcastCode;
    };

    TEST_F(TestBoardProtectionFaultNotifier, constructor_registers_board_protection_with_platform)
    {
        GivenNotifierConstructed();
        // EXPECT_CALL above asserts RegisterBoardProtection was called exactly once
    }

    TEST_F(TestBoardProtectionFaultNotifier, over_current_maps_to_overcurrent_fault_code)
    {
        GivenNotifierConstructed();
        notifier->Register([this](state_machine::FaultCode code)
            {
                receivedStateMachineCode = code;
            });

        TriggerProtection(application::PlatformFactory::BoardProtectionReason::overCurrent);

        ASSERT_TRUE(receivedStateMachineCode.has_value());
        EXPECT_EQ(*receivedStateMachineCode, state_machine::FaultCode::overcurrent);
    }

    TEST_F(TestBoardProtectionFaultNotifier, over_voltage_maps_to_overvoltage_fault_code)
    {
        GivenNotifierConstructed();
        notifier->Register([this](state_machine::FaultCode code)
            {
                receivedStateMachineCode = code;
            });

        TriggerProtection(application::PlatformFactory::BoardProtectionReason::overVoltage);

        ASSERT_TRUE(receivedStateMachineCode.has_value());
        EXPECT_EQ(*receivedStateMachineCode, state_machine::FaultCode::overvoltage);
    }

    TEST_F(TestBoardProtectionFaultNotifier, over_temperature_maps_to_overtemperature_fault_code)
    {
        GivenNotifierConstructed();
        notifier->Register([this](state_machine::FaultCode code)
            {
                receivedStateMachineCode = code;
            });

        TriggerProtection(application::PlatformFactory::BoardProtectionReason::overTemperature);

        ASSERT_TRUE(receivedStateMachineCode.has_value());
        EXPECT_EQ(*receivedStateMachineCode, state_machine::FaultCode::overtemperature);
    }

    TEST_F(TestBoardProtectionFaultNotifier, broadcast_sink_receives_same_fault_code_as_state_machine_sink)
    {
        GivenNotifierConstructed();
        notifier->Register([this](state_machine::FaultCode code)
            {
                receivedStateMachineCode = code;
            });
        notifier->OnFaultBroadcast([this](state_machine::FaultCode code)
            {
                receivedBroadcastCode = code;
            });

        TriggerProtection(application::PlatformFactory::BoardProtectionReason::overCurrent);

        ASSERT_TRUE(receivedStateMachineCode.has_value());
        ASSERT_TRUE(receivedBroadcastCode.has_value());
        EXPECT_EQ(*receivedStateMachineCode, *receivedBroadcastCode);
        EXPECT_EQ(*receivedBroadcastCode, state_machine::FaultCode::overcurrent);
    }

    TEST_F(TestBoardProtectionFaultNotifier, firing_with_no_sink_registered_does_not_crash)
    {
        GivenNotifierConstructed();

        EXPECT_NO_FATAL_FAILURE(
            TriggerProtection(application::PlatformFactory::BoardProtectionReason::overCurrent));
    }

    TEST_F(TestBoardProtectionFaultNotifier, firing_with_only_broadcast_sink_does_not_crash)
    {
        GivenNotifierConstructed();
        notifier->OnFaultBroadcast([this](state_machine::FaultCode code)
            {
                receivedBroadcastCode = code;
            });

        EXPECT_NO_FATAL_FAILURE(
            TriggerProtection(application::PlatformFactory::BoardProtectionReason::overVoltage));

        ASSERT_TRUE(receivedBroadcastCode.has_value());
        EXPECT_EQ(*receivedBroadcastCode, state_machine::FaultCode::overvoltage);
    }
}
