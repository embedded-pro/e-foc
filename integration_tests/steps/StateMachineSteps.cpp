#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

WHEN(R"(the calibrate command is issued)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId,
        {}, std::chrono::seconds{ 30 });
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
