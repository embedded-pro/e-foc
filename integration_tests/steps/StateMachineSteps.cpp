#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

GIVEN(R"(the system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId))
        << "Electrical identification rejected — check state machine is in Idle";
}

STEP(R"(the enable command is issued)")
{
    GTEST_SKIP() << "Direct internal enable not available — use 'the CAN Start command is received'";
}

STEP(R"(a hardware fault is raised by the platform)")
{
    GTEST_SKIP() << "Platform fault injection not available via transport";
}

WHEN(R"(the calibrate command is issued)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId);
}

WHEN(R"(the disable command is issued)")
{
    GTEST_SKIP() << "Direct internal disable not available — use 'the CAN Stop command is received'";
}

WHEN(R"(the clear fault command is issued)")
{
    GTEST_SKIP() << "Direct internal clear-fault not available — use 'the CAN ClearFault command is received'";
}

THEN(R"(the state machine shall be in the Idle state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::idle))
        << "Motor state is not idle/ready over CAN telemetry";
}

THEN(R"(the state machine shall be in the Calibrating state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::calibrating))
        << "Motor state is not calibrating over CAN telemetry";
}

THEN(R"(the state machine shall be in the Ready state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::idle))
        << "Motor state is not idle/ready over CAN telemetry";
}

THEN(R"(the state machine shall be in the Enabled state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running))
        << "Motor state is not running/enabled over CAN telemetry";
}

THEN(R"(the state machine shall be in the Fault state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::fault))
        << "Motor state is not fault over CAN telemetry";
}
