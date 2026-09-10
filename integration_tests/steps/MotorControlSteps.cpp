#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <cstdint>
#include <gtest/gtest.h>

using namespace integration;

namespace
{
    hal::Can::Message EncodeFixed16BE(float value, int32_t scale)
    {
        const auto raw = static_cast<int16_t>(value * static_cast<float>(scale));
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(static_cast<uint16_t>(raw) >> 8));
        msg.push_back(static_cast<uint8_t>(raw & 0xFF));
        return msg;
    }
}

WHEN(R"(the motor is enabled)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId))
        << "Enable (focStartId) command rejected";
    // Check if the motor immediately goes back to idle (fault/emergency stop)
    const bool isIdle = fixture.WaitForMotorState(can::FocMotorState::idle, std::chrono::milliseconds{500});
    EXPECT_FALSE(isIdle) << "Motor immediately returned to idle after enable (fault or emergency stop?)";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running, std::chrono::seconds{5}))
        << "Motor did not reach running state after enable";
}

WHEN(R"(the motor is disabled)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.DisableMotor()) << "Motor did not return to idle after disable";
}

WHEN(R"(a torque setpoint of {float} A is applied)", (float iq))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetTorqueSetpointId,
        EncodeFixed16BE(iq, can::focCurrentScale)))
        << "Torque setpoint command rejected";
}

WHEN(R"(a speed setpoint of {float} rps is applied)", (float rps))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetSpeedSetpointId,
        EncodeFixed16BE(rps, can::focSpeedScale)))
        << "Speed setpoint command rejected";
}

WHEN(R"(a position setpoint of {float} rad is applied)", (float rad))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetPositionSetpointId,
        EncodeFixed16BE(rad, can::focPositionScale)))
        << "Position setpoint command rejected";
}

THEN(R"(the state machine shall be in the running state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running))
        << "Motor is not in running state over CAN telemetry";
}
