#include "can-lite/core/CanPayload.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "e-foc-protocol/contract/FocMotorWireContract.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <cstdint>
#include <gtest/gtest.h>

using namespace integration;

namespace
{
    hal::Can::Message EncodeFloat32(float value)
    {
        services::CanPayloadWriter payload;
        payload.WriteFloat32(value);
        return payload.Message();
    }
}

WHEN(R"(the motor is enabled)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId))
        << "Enable (focStartId) command rejected";
    const bool isReady = fixture.WaitForMotorState(can::FocMotorState::ready, std::chrono::milliseconds{ 500 });
    EXPECT_FALSE(isReady) << "Motor immediately returned to ready after enable (fault or emergency stop?)";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running, std::chrono::seconds{ 5 }))
        << "Motor did not reach running state after enable";
}

WHEN(R"(the motor is started from the encoder index)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId, {}, std::chrono::seconds{ 30 }))
        << "Start refused: the index search did not restore the rotor reference";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running, std::chrono::seconds{ 5 }))
        << "Motor did not reach running state after the index search";
}

WHEN(R"(the motor is disabled)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.DisableMotor()) << "Motor did not return to ready after disable";
}

WHEN(R"(a torque setpoint of {float} A is applied)", (float iq))
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();
    const auto commandsBefore = setup.trace.Commands().size();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetTorqueSetpointId,
        EncodeFloat32(iq)))
        << "Torque setpoint command rejected";
    setup.lastSetpoint = Setpoint{ can::focSetTorqueSetpointId, iq, fixture.ServerNodeId(), commandsBefore };
}

WHEN(R"(a speed setpoint of {float} rad\/s is applied)", (float radiansPerSecond))
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();
    const auto commandsBefore = setup.trace.Commands().size();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetSpeedSetpointId,
        EncodeFloat32(radiansPerSecond)))
        << "Speed setpoint command rejected";
    setup.lastSetpoint = Setpoint{ can::focSetSpeedSetpointId, radiansPerSecond, fixture.ServerNodeId(), commandsBefore };
}

WHEN(R"(a position setpoint of {float} rad is applied)", (float rad))
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();
    const auto commandsBefore = setup.trace.Commands().size();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetPositionSetpointId,
        EncodeFloat32(rad)))
        << "Position setpoint command rejected";
    setup.lastSetpoint = Setpoint{ can::focSetPositionSetpointId, rad, fixture.ServerNodeId(), commandsBefore };
}

THEN(R"(a position setpoint of {float} rad shall be refused)", (float rad))
{
    auto& fixture = context.Get<Fixture>();
    EXPECT_FALSE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetPositionSetpointId,
        EncodeFloat32(rad)))
        << "The target accepted a position setpoint of " << rad << " rad";
}

THEN(R"(the state machine shall be in the running state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running))
        << "Motor is not in running state over CAN telemetry";
}

THEN(R"(the state machine shall report the stopping state)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::stopping, std::chrono::milliseconds{ 500 }))
        << "Motor did not report stopping after the quick stop";
}

THEN(R"(the state machine shall be in the ready state within {int} ms)", (int timeoutMs))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::ready, std::chrono::milliseconds{ timeoutMs }))
        << "Motor did not reach ready within " << timeoutMs << " ms";
}

THEN(R"(the motor shall refuse to enable)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId);
    ASSERT_FALSE(fixture.WaitForMotorState(can::FocMotorState::running, std::chrono::seconds{ 2 }))
        << "Motor reached running state without a complete calibration";
}
