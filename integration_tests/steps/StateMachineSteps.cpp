#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/FocIntegrationFixture.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

GIVEN(R"(the system is initialised with no valid calibration data)")
{
    context.Emplace<FocIntegrationFixture>();
    context.Get<FocIntegrationFixture>().ConstructWithInvalidNvm();
}

GIVEN(R"(the system is initialised with valid calibration data)")
{
    context.Emplace<FocIntegrationFixture>();
    context.Get<FocIntegrationFixture>().ConstructWithValidNvm();
}

STEP(R"(the enable command is issued)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.motorStateMachine->CmdEnable();
    fixture.ExecuteAllActions();
}

STEP(R"(a hardware fault is raised by the platform)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    fixture.ExecuteAllActions();
}

WHEN(R"(the calibrate command is issued)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    if (!fixture.calibrationExpectationsConfigured)
    {
        fixture.SetupCalibrationExpectations();
    }
    fixture.motorStateMachine->CmdCalibrate();
    fixture.ExecuteAllActions();
}

WHEN(R"(the disable command is issued)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.motorStateMachine->CmdDisable();
    fixture.ExecuteAllActions();
}

WHEN(R"(the clear fault command is issued)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.motorStateMachine->CmdClearFault();
    fixture.ExecuteAllActions();
}

THEN(R"(the state machine shall be in the Idle state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Idle>(
            context.Get<FocIntegrationFixture>().motorStateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Calibrating state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Calibrating>(
            context.Get<FocIntegrationFixture>().motorStateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Ready state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Ready>(
            context.Get<FocIntegrationFixture>().motorStateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Enabled state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Enabled>(
            context.Get<FocIntegrationFixture>().motorStateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Fault state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Fault>(
            context.Get<FocIntegrationFixture>().motorStateMachine->CurrentState()));
}
