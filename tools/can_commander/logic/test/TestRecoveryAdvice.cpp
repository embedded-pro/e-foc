#include "tools/can_commander/logic/RecoveryAdvice.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;
    using can::FocMotorMode;
    using can::FocMotorState;
    using testing::HasSubstr;

    class TestRecoveryAdvice
        : public testing::Test
    {
    public:
        static std::optional<RecoveryAdvice> Refuse(uint8_t commandType, std::optional<FocMotorState> state, Refusal refusal = Refusal::invalidState,
            std::optional<FocMotorMode> mode = FocMotorMode::torque)
        {
            return AdviseRecovery(RefusedCommand{ commandType, refusal, state, mode, "Over current" });
        }
    };
}

TEST_F(TestRecoveryAdvice, start_refused_while_idle_asks_to_align_the_rotor)
{
    const auto advice = Refuse(can::focStartId, FocMotorState::idle);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::alignRotor, advice->action);
    EXPECT_THAT(advice->text, HasSubstr("Align the rotor"));
}

TEST_F(TestRecoveryAdvice, setpoint_refused_before_any_status_asks_to_align_the_rotor)
{
    const auto advice = Refuse(can::focSetTorqueSetpointId, std::nullopt);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::alignRotor, advice->action);
}

TEST_F(TestRecoveryAdvice, stop_or_clear_fault_refused_while_idle_needs_no_advice)
{
    EXPECT_FALSE(Refuse(can::focStopId, FocMotorState::idle).has_value());
    EXPECT_FALSE(Refuse(can::focClearFaultId, FocMotorState::idle).has_value());
}

TEST_F(TestRecoveryAdvice, any_operating_command_refused_in_fault_offers_to_clear_it)
{
    for (const auto command : { can::focStartId, can::focStopId, can::focSetSpeedSetpointId })
    {
        const auto advice = Refuse(command, FocMotorState::fault);

        ASSERT_TRUE(advice.has_value());
        EXPECT_EQ(RecoveryAction::clearFault, advice->action);
        EXPECT_THAT(advice->text, HasSubstr("(Over current)"));
    }
}

TEST_F(TestRecoveryAdvice, clear_fault_refused_in_fault_asks_to_remove_the_cause)
{
    const auto advice = Refuse(can::focClearFaultId, FocMotorState::fault);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::none, advice->action);
    EXPECT_THAT(advice->text, HasSubstr("restart the controller"));
}

TEST_F(TestRecoveryAdvice, commands_refused_while_calibrating_ask_to_wait)
{
    const auto advice = Refuse(can::focStartId, FocMotorState::calibrating);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::none, advice->action);
    EXPECT_THAT(advice->text, HasSubstr("Wait"));
}

TEST_F(TestRecoveryAdvice, partial_calibration_points_to_the_motor_parameters)
{
    const auto advice = Refuse(can::focStartId, FocMotorState::partialCalibration, Refusal::invalidState, FocMotorMode::speed);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::showMotorParameters, advice->action);
    EXPECT_THAT(advice->text, HasSubstr("speed mode"));
}

TEST_F(TestRecoveryAdvice, start_refused_while_running_needs_no_advice)
{
    EXPECT_FALSE(Refuse(can::focStartId, FocMotorState::running).has_value());
}

TEST_F(TestRecoveryAdvice, settings_refusals_are_left_to_their_own_page)
{
    EXPECT_FALSE(Refuse(can::focSetParameterId, FocMotorState::fault).has_value());
    EXPECT_FALSE(Refuse(can::focSetParameterId, FocMotorState::idle, Refusal::modeMismatch).has_value());
}

TEST_F(TestRecoveryAdvice, setpoint_for_another_mode_names_both_modes)
{
    const auto advice = Refuse(can::focSetSpeedSetpointId, FocMotorState::idle, Refusal::modeMismatch, FocMotorMode::position);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::none, advice->action);
    EXPECT_THAT(advice->text, HasSubstr("speed setpoint applies in speed mode only"));
    EXPECT_THAT(advice->text, HasSubstr("runs position mode"));
}

TEST_F(TestRecoveryAdvice, align_refused_while_stopped_asks_for_a_calibration)
{
    const auto advice = Refuse(can::focAlignId, FocMotorState::idle, Refusal::modeMismatch, FocMotorMode::speed);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::showMotorParameters, advice->action);
    EXPECT_THAT(advice->text, HasSubstr("no complete calibration for speed mode"));
}

TEST_F(TestRecoveryAdvice, align_refused_while_running_asks_to_stop_first)
{
    const auto advice = Refuse(can::focAlignId, FocMotorState::running, Refusal::modeMismatch);

    ASSERT_TRUE(advice.has_value());
    EXPECT_EQ(RecoveryAction::stopMotor, advice->action);
}

TEST_F(TestRecoveryAdvice, align_refused_while_calibrating_or_faulted_follows_the_state)
{
    const auto calibrating = Refuse(can::focAlignId, FocMotorState::calibrating, Refusal::modeMismatch);
    const auto faulted = Refuse(can::focAlignId, FocMotorState::fault, Refusal::modeMismatch);

    ASSERT_TRUE(calibrating.has_value());
    EXPECT_THAT(calibrating->text, HasSubstr("Wait"));
    ASSERT_TRUE(faulted.has_value());
    EXPECT_EQ(RecoveryAction::clearFault, faulted->action);
}

TEST_F(TestRecoveryAdvice, names_every_action_like_its_button)
{
    EXPECT_EQ("Align Rotor", RecoveryActionName(RecoveryAction::alignRotor));
    EXPECT_EQ("Clear Fault", RecoveryActionName(RecoveryAction::clearFault));
    EXPECT_EQ("Stop Motor", RecoveryActionName(RecoveryAction::stopMotor));
    EXPECT_EQ("Open Motor Parameters", RecoveryActionName(RecoveryAction::showMotorParameters));
    EXPECT_TRUE(RecoveryActionName(RecoveryAction::none).empty());
}
