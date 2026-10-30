#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/logic/CommandDescriptions.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;

    class TestCommandDescriptions
        : public testing::Test
    {};
}

TEST_F(TestCommandDescriptions, names_motor_commands_like_the_gui)
{
    EXPECT_EQ("Start Motor", CommandDisplayName(can::focMotorCategoryId, can::focStartId));
    EXPECT_EQ("Emergency Stop", CommandDisplayName(can::focMotorCategoryId, can::focEmergencyStopId));
    EXPECT_EQ("Torque Setpoint", CommandDisplayName(can::focMotorCategoryId, can::focSetTorqueSetpointId));
    EXPECT_EQ("Position Loop Bandwidth", CommandDisplayName(can::focMotorCategoryId, can::focSetPidPositionId));
}

TEST_F(TestCommandDescriptions, has_no_name_outside_motor_category)
{
    EXPECT_TRUE(CommandDisplayName(services::canSystemCategoryId, can::focStartId).empty());
    EXPECT_TRUE(CommandDisplayName(can::focMotorCategoryId, 0x7e).empty());
}

TEST_F(TestCommandDescriptions, describes_acknowledgements_in_plain_language)
{
    EXPECT_EQ("accepted", AckStatusDescription(services::CanAckStatus::success));
    EXPECT_EQ("rejected: not allowed in the motor's current state", AckStatusDescription(services::CanAckStatus::invalidState));
    EXPECT_EQ("unknown acknowledgement", AckStatusDescription(static_cast<services::CanAckStatus>(0xff)));
}

TEST_F(TestCommandDescriptions, describes_rejections_in_plain_language)
{
    EXPECT_EQ("rejected: not valid in the active control mode", RejectionDescription(can::FocMotorCategoryError::modeMismatch));
    EXPECT_EQ("rejected for an unknown reason", RejectionDescription(static_cast<can::FocMotorCategoryError>(0xff)));
}

TEST_F(TestCommandDescriptions, names_commissioning_and_settings_commands)
{
    EXPECT_EQ("Align Rotor", CommandDisplayName(can::focMotorCategoryId, can::focAlignId));
    EXPECT_EQ("Identify Electrical", CommandDisplayName(can::focMotorCategoryId, can::focIdentifyElectricalId));
    EXPECT_EQ("Encoder Resolution", CommandDisplayName(can::focMotorCategoryId, can::focSetEncoderResolutionId));
    EXPECT_EQ("Controller Selection", CommandDisplayName(can::focMotorCategoryId, can::focSelectAlgorithmId));
    EXPECT_EQ("Setting", CommandDisplayName(can::focMotorCategoryId, can::focSetParameterId));
    EXPECT_EQ("Calibrate", CommandDisplayName(can::focMotorCategoryId, can::focCalibrateId));
    EXPECT_EQ("Clear Calibration", CommandDisplayName(can::focMotorCategoryId, can::focClearCalibrationId));
    EXPECT_EQ("Clear Diagnostics", CommandDisplayName(can::focMotorCategoryId, can::focClearDiagnosticsId));
}

TEST_F(TestCommandDescriptions, explains_a_design_that_cannot_be_made)
{
    EXPECT_EQ("rejected: the controller cannot be designed from the motor parameters and settings it has",
        RejectionDescription(can::FocMotorCategoryError::invalidParameters));
}
