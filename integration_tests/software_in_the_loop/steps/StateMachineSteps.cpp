#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/FocIntegrationFixture.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

static void PopulateTorqueAccessor(StateMachineAccessor& accessor, FocIntegrationFixture& fixture)
{
    accessor.stateMachine = &*fixture.motorStateMachine;
    accessor.executeAll = [&fixture]()
        {
            fixture.ExecuteAllActions();
        };
    accessor.setupCalibrationExpectations = [&fixture]()
        {
            fixture.SetupCalibrationExpectations();
        };
    accessor.setupCanIntegration = [&fixture]()
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
    accessor.injectCanClearFault = [&fixture]()
        {
            fixture.InjectCanClearFault();
        };
    accessor.triggerHardwareFault = [&fixture]()
        {
            fixture.faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
            fixture.ExecuteAllActions();
        };
    accessor.completePolePairsEstimation = [&fixture](std::size_t polePairs)
        {
            fixture.CompletePolePairsEstimation(polePairs);
        };
    accessor.completeRLEstimation = [&fixture](foc::Ohm r, foc::MilliHenry l)
        {
            fixture.CompleteRLEstimation(r, l);
        };
    accessor.completeAlignment = [&fixture](foc::Radians offset)
        {
            fixture.CompleteAlignment(offset);
        };
    accessor.completePolePairsFailure = [&fixture]()
        {
            fixture.capturedPolePairsCallback(std::nullopt);
            fixture.ExecuteAllActions();
        };
}

GIVEN(R"(the system is initialised with no valid calibration data)")
{
    context.Emplace<FocIntegrationFixture>();
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.ConstructWithInvalidNvm();

    context.Emplace<StateMachineAccessor>();
    PopulateTorqueAccessor(context.Get<StateMachineAccessor>(), fixture);
}

GIVEN(R"(the system is initialised with valid calibration data)")
{
    context.Emplace<FocIntegrationFixture>();
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.ConstructWithValidNvm();

    context.Emplace<StateMachineAccessor>();
    PopulateTorqueAccessor(context.Get<StateMachineAccessor>(), fixture);
}

STEP(R"(the enable command is issued)")
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.stateMachine->CmdEnable();
    accessor.executeAll();
}

STEP(R"(a hardware fault is raised by the platform)")
{
    context.Get<StateMachineAccessor>().triggerHardwareFault();
}

WHEN(R"(the calibrate command is issued)")
{
    auto& accessor = context.Get<StateMachineAccessor>();
    if (!accessor.calibrationExpectationsConfigured)
    {
        accessor.setupCalibrationExpectations();
        accessor.calibrationExpectationsConfigured = true;
    }
    accessor.stateMachine->CmdCalibrate();
    accessor.executeAll();
}

WHEN(R"(the disable command is issued)")
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.stateMachine->CmdDisable();
    accessor.executeAll();
}

WHEN(R"(the clear fault command is issued)")
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.stateMachine->CmdClearFault();
    accessor.executeAll();
}

THEN(R"(the state machine shall be in the Idle state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Idle>(
            context.Get<StateMachineAccessor>().stateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Calibrating state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Calibrating>(
            context.Get<StateMachineAccessor>().stateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Ready state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Ready>(
            context.Get<StateMachineAccessor>().stateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Enabled state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Enabled>(
            context.Get<StateMachineAccessor>().stateMachine->CurrentState()));
}

THEN(R"(the state machine shall be in the Fault state)")
{
    EXPECT_TRUE(
        std::holds_alternative<state_machine::Fault>(
            context.Get<StateMachineAccessor>().stateMachine->CurrentState()));
}
