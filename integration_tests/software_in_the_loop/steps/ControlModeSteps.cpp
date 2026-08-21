#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/ControlModeCoordinationFixture.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

using namespace testing;
using namespace integration;

namespace
{
    can::FocMotorMode ModeFromString(const std::string& mode)
    {
        if (mode == "torque")
            return can::FocMotorMode::torque;
        if (mode == "speed")
            return can::FocMotorMode::speed;
        if (mode == "position")
            return can::FocMotorMode::position;
        throw std::invalid_argument("Unknown control mode: " + mode);
    }

    state_machine::ControlMode ControlModeFromString(const std::string& mode)
    {
        if (mode == "torque")
            return state_machine::ControlMode::torque;
        if (mode == "speed")
            return state_machine::ControlMode::speed;
        if (mode == "position")
            return state_machine::ControlMode::position;
        throw std::invalid_argument("Unknown control mode: " + mode);
    }

    services::CanAckStatus StatusFromString(const std::string& reason)
    {
        if (reason == "ok" || reason == "success")
            return services::CanAckStatus::success;
        if (reason == "invalidPayload")
            return services::CanAckStatus::invalidPayload;
        if (reason == "invalidState")
            return services::CanAckStatus::invalidState;
        if (reason == "notImplemented")
            return services::CanAckStatus::notImplemented;
        if (reason == "busy" || reason == "controlModeMismatch" || reason == "modeMismatch" || reason == "nvmFailed" || reason == "persistenceFailed" || reason == "categoryError")
            return services::CanAckStatus::categoryError;
        throw std::invalid_argument("Unknown ack status: " + reason);
    }

    can::FocMotorCategoryError CategoryErrorFromString(const std::string& reason)
    {
        if (reason == "busy")
            return can::FocMotorCategoryError::busy;
        if (reason == "nvmFailed" || reason == "persistenceFailed")
            return can::FocMotorCategoryError::persistenceFailed;
        if (reason == "controlModeMismatch" || reason == "modeMismatch")
            return can::FocMotorCategoryError::modeMismatch;
        if (reason == "calibrationFailed")
            return can::FocMotorCategoryError::calibrationFailed;
        if (reason == "abortedByFault")
            return can::FocMotorCategoryError::abortedByFault;
        throw std::invalid_argument("Unknown category-error reason: " + reason);
    }

    void PopulateCoordinatorAccessor(StateMachineAccessor& accessor, ControlModeCoordinationFixture& fixture)
    {
        accessor.stateMachine = nullptr;
        accessor.executeAll = [&fixture]()
        {
            fixture.ExecuteAllActions();
        };
        accessor.setupCanIntegrationWithCoordinator = [&fixture]()
        {
            fixture.SetupCanIntegration();
        };
        accessor.injectCanStart = [&fixture]()
        {
            fixture.InjectCanStart();
        };
        accessor.injectCanStop = [&fixture]()
        {
            fixture.InjectCanStop();
        };
        accessor.injectCanSelectControlMode = [&fixture](can::FocMotorMode mode)
        {
            fixture.InjectCanSelectControlMode(mode);
        };
        accessor.injectCanSetTorqueSetpoint = [&fixture](int16_t value)
        {
            fixture.InjectCanSetTorqueSetpoint(value);
        };
        accessor.injectCanSetSpeedSetpoint = [&fixture](int16_t value)
        {
            fixture.InjectCanSetSpeedSetpoint(value);
        };
        accessor.injectCanSetPositionSetpoint = [&fixture](int16_t value)
        {
            fixture.InjectCanSetPositionSetpoint(value);
        };
        accessor.getActiveMode = [&fixture]() -> state_machine::ControlMode
        {
            return fixture.coordinator->Active();
        };
        accessor.wasCommandAckSent = [&fixture]() -> bool
        {
            return fixture.commandAckSent;
        };
        accessor.wasSelectResponseSent = [&fixture]() -> bool
        {
            return fixture.selectResponseSent;
        };
        accessor.lastCommandAckStatus = [&fixture]() -> services::CanAckStatus
        {
            return fixture.lastCommandAckStatus;
        };
        accessor.lastCommandAckMessageType = [&fixture]() -> uint8_t
        {
            return fixture.lastCommandAckMessageType;
        };
        accessor.wasCategoryErrorSent = [&fixture]() -> bool
        {
            return fixture.categoryErrorSent;
        };
        accessor.lastCategoryErrorReason = [&fixture]() -> can::FocMotorCategoryError
        {
            return fixture.lastCategoryErrorReason;
        };
        accessor.lastCategoryErrorOriginCmd = [&fixture]() -> uint8_t
        {
            return fixture.lastCategoryErrorOriginCmd;
        };
        accessor.nvmWriteCount = [&fixture]() -> std::size_t
        {
            return fixture.eepromStub.writeCount;
        };
        accessor.restartSystem = [&fixture]()
        {
            fixture.RestartCoordinator();
        };
    }
}

GIVEN(R"(the multi-mode system is initialised with valid calibration data)")
{
    context.Emplace<ControlModeCoordinationFixture>();
    auto& fixture = context.Get<ControlModeCoordinationFixture>();
    fixture.ConstructCoordinator();
    context.Emplace<StateMachineAccessor>();
    PopulateCoordinatorAccessor(context.Get<StateMachineAccessor>(), fixture);
}

GIVEN(R"(the CAN category server is connected to the control mode coordinator)")
{
    context.Get<StateMachineAccessor>().setupCanIntegrationWithCoordinator();
}

GIVEN(R"(the active control mode is {word})", (std::string mode))
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.injectCanSelectControlMode(ModeFromString(mode));
    accessor.executeAll();
}

WHEN(R"(the CAN SelectControlMode command is received with mode {word})", (std::string mode))
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.injectCanSelectControlMode(ModeFromString(mode));
    accessor.executeAll();
}

WHEN(R"(the CAN SetTorqueSetpoint command is received with value {int})", (int32_t value))
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.nvmWriteBaseline = accessor.nvmWriteCount();
    accessor.injectCanSetTorqueSetpoint(static_cast<int16_t>(value));
    accessor.executeAll();
}

WHEN(R"(the CAN SetSpeedSetpoint command is received with value {int})", (int32_t value))
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.nvmWriteBaseline = accessor.nvmWriteCount();
    accessor.injectCanSetSpeedSetpoint(static_cast<int16_t>(value));
    accessor.executeAll();
}

WHEN(R"(the CAN SetPositionSetpoint command is received with value {int})", (int32_t value))
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.nvmWriteBaseline = accessor.nvmWriteCount();
    accessor.injectCanSetPositionSetpoint(static_cast<int16_t>(value));
    accessor.executeAll();
}

WHEN(R"(the system is restarted)")
{
    context.Get<StateMachineAccessor>().restartSystem();
}

THEN(R"(the active control mode shall be {word})", (std::string mode))
{
    const auto expected = ControlModeFromString(mode);
    const auto actual = context.Get<StateMachineAccessor>().getActiveMode();
    EXPECT_EQ(actual, expected);
}

THEN(R"(a SelectControlModeResponse shall be emitted)")
{
    EXPECT_TRUE(context.Get<StateMachineAccessor>().wasSelectResponseSent());
}

THEN(R"(a SelectControlModeResponse shall be emitted with reason {word})", (std::string reason))
{
    const auto& accessor = context.Get<StateMachineAccessor>();
    const auto expected = StatusFromString(reason);
    EXPECT_TRUE(accessor.wasCommandAckSent());
    EXPECT_EQ(accessor.lastCommandAckStatus(), expected);
    if (expected == services::CanAckStatus::success)
        EXPECT_TRUE(accessor.wasSelectResponseSent());
    else if (expected == services::CanAckStatus::categoryError)
    {
        EXPECT_TRUE(accessor.wasCategoryErrorSent());
        EXPECT_EQ(accessor.lastCategoryErrorReason(), CategoryErrorFromString(reason));
    }
}

THEN(R"(a CommandRejected frame shall be emitted with reason controlModeMismatch)")
{
    const auto& accessor = context.Get<StateMachineAccessor>();
    EXPECT_TRUE(accessor.wasCommandAckSent());
    EXPECT_EQ(accessor.lastCommandAckStatus(), services::CanAckStatus::categoryError);
    EXPECT_TRUE(accessor.wasCategoryErrorSent());
    EXPECT_EQ(accessor.lastCategoryErrorReason(), can::FocMotorCategoryError::modeMismatch);
}

THEN(R"(no CommandRejected frame shall be emitted)")
{
    const auto& accessor = context.Get<StateMachineAccessor>();
    if (accessor.wasCommandAckSent())
        EXPECT_EQ(accessor.lastCommandAckStatus(), services::CanAckStatus::success);
}

THEN(R"(no NVM write shall occur)")
{
    const auto& accessor = context.Get<StateMachineAccessor>();
    EXPECT_EQ(accessor.nvmWriteCount(), accessor.nvmWriteBaseline);
}
