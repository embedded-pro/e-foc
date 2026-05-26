#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/ControlModeCoordinationFixture.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace testing;
using namespace integration;

namespace
{
    services::FocMotorMode ModeFromString(const std::string& mode)
    {
        if (mode == "torque")
            return services::FocMotorMode::torque;
        if (mode == "speed")
            return services::FocMotorMode::speed;
        if (mode == "position")
            return services::FocMotorMode::position;
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

    services::FocRejectReason ReasonFromString(const std::string& reason)
    {
        if (reason == "ok")
            return services::FocRejectReason::ok;
        if (reason == "busy")
            return services::FocRejectReason::busy;
        if (reason == "controlModeMismatch")
            return services::FocRejectReason::controlModeMismatch;
        if (reason == "nvmFailed")
            return services::FocRejectReason::nvmFailed;
        if (reason == "invalidPayload")
            return services::FocRejectReason::invalidPayload;
        throw std::invalid_argument("Unknown reject reason: " + reason);
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
        accessor.injectCanSelectControlMode = [&fixture](services::FocMotorMode mode)
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
        accessor.wasCommandRejectedSent = [&fixture]() -> bool
        {
            return fixture.commandRejectedSent;
        };
        accessor.wasSelectResponseSent = [&fixture]() -> bool
        {
            return fixture.selectResponseSent;
        };
        accessor.lastCommandRejectedReason = [&fixture]() -> services::FocRejectReason
        {
            return fixture.lastCommandRejectedReason;
        };
        accessor.lastSelectResponseReason = [&fixture]() -> services::FocRejectReason
        {
            return fixture.lastSelectResponseReason;
        };
        accessor.nvmWriteCount = [&fixture]() -> int
        {
            return fixture.nvmSaveCount;
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
    accessor.injectCanSetTorqueSetpoint(static_cast<int16_t>(value));
    accessor.executeAll();
}

WHEN(R"(the CAN SetSpeedSetpoint command is received with value {int})", (int32_t value))
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.injectCanSetSpeedSetpoint(static_cast<int16_t>(value));
    accessor.executeAll();
}

WHEN(R"(the CAN SetPositionSetpoint command is received with value {int})", (int32_t value))
{
    auto& accessor = context.Get<StateMachineAccessor>();
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
    EXPECT_TRUE(accessor.wasSelectResponseSent());
    EXPECT_EQ(accessor.lastSelectResponseReason(), ReasonFromString(reason));
}

THEN(R"(a CommandRejected frame shall be emitted with reason controlModeMismatch)")
{
    const auto& accessor = context.Get<StateMachineAccessor>();
    EXPECT_TRUE(accessor.wasCommandRejectedSent());
    EXPECT_EQ(accessor.lastCommandRejectedReason(), services::FocRejectReason::controlModeMismatch);
}

THEN(R"(no CommandRejected frame shall be emitted)")
{
    EXPECT_FALSE(context.Get<StateMachineAccessor>().wasCommandRejectedSent());
}

THEN(R"(no NVM write shall occur)")
{
    EXPECT_EQ(context.Get<StateMachineAccessor>().nvmWriteCount(), 0);
}
