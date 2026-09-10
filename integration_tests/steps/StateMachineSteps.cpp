#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the {word} motor system is initialised with no valid calibration data)", (std::string))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

WHEN(R"(the {word} control mode is selected)", (std::string mode))
{
    auto& fixture = context.Get<Fixture>();
    can::FocMotorMode motorMode = can::FocMotorMode::torque;
    if (mode == "speed")
        motorMode = can::FocMotorMode::speed;
    else if (mode == "position")
        motorMode = can::FocMotorMode::position;
    ASSERT_TRUE(fixture.SelectControlMode(motorMode)) << "Control mode selection failed";
}

WHEN(R"(the calibrate command is issued)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId,
        {}, std::chrono::seconds{ 60 }))
        << "Calibrate command did not complete within 60s";
}

THEN(R"(the state machine shall be in the Ready state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::idle))
        << "Motor state is not idle/ready over CAN telemetry";
}

THEN(R"(the state machine shall be in the Fault state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::fault))
        << "Motor state is not fault over CAN telemetry";
}
