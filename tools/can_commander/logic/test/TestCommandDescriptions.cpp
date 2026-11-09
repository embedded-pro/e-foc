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
    EXPECT_EQ("Torque Setpoint (N\xC2\xB7m)", CommandDisplayName(can::focMotorCategoryId, can::focSetTorqueSetpointNewtonMeterId));
    EXPECT_EQ("Position Setpoint", CommandDisplayName(can::focMotorCategoryId, can::focSetPositionSetpointId));
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
    EXPECT_EQ("Controller Selection", CommandDisplayName(can::focMotorCategoryId, can::focSelectAlgorithmId));
    EXPECT_EQ("Setting", CommandDisplayName(can::focMotorCategoryId, can::focSetParameterId));
    EXPECT_EQ("Calibrate", CommandDisplayName(can::focMotorCategoryId, can::focCalibrateId));
    EXPECT_EQ("Clear Calibration", CommandDisplayName(can::focMotorCategoryId, can::focClearCalibrationId));
    EXPECT_EQ("Clear Diagnostics", CommandDisplayName(can::focMotorCategoryId, can::focClearDiagnosticsId));
    EXPECT_EQ("Fault History Query", CommandDisplayName(can::focMotorCategoryId, can::focQueryFaultHistoryId));
    EXPECT_EQ("Clear Fault History", CommandDisplayName(can::focMotorCategoryId, can::focClearFaultHistoryId));
    EXPECT_EQ("Recalibrate", CommandDisplayName(can::focMotorCategoryId, can::focRecalibrateId));
    EXPECT_EQ("Reset Model", CommandDisplayName(can::focMotorCategoryId, can::focResetModelId));
}

TEST_F(TestCommandDescriptions, explains_a_design_that_cannot_be_made)
{
    EXPECT_EQ("rejected: the controller cannot be designed from the motor parameters and settings it has",
        RejectionDescription(can::FocMotorCategoryError::invalidParameters));
}

TEST_F(TestCommandDescriptions, names_every_motor_command)
{
    const std::pair<uint8_t, std::string_view> names[]{
        { can::focStartId, "Start Motor" },
        { can::focStopId, "Stop Motor" },
        { can::focEmergencyStopId, "Emergency Stop" },
        { can::focClearFaultId, "Clear Fault" },
        { can::focSelectControlModeId, "Control Mode" },
        { can::focSetTorqueSetpointId, "Torque Setpoint" },
        { can::focSetTorqueSetpointNewtonMeterId, "Torque Setpoint (N\xC2\xB7m)" },
        { can::focSetSpeedSetpointId, "Speed Setpoint" },
        { can::focSetPositionSetpointId, "Position Setpoint" },
        { can::focQueryContractVersionId, "Firmware Compatibility Check" },
        { can::focAlignId, "Align Rotor" },
        { can::focIdentifyElectricalId, "Identify Electrical" },
        { can::focRequestTelemetryId, "Status Request" },
        { can::focSelectAlgorithmId, "Controller Selection" },
        { can::focQueryAlgorithmsId, "Controller Query" },
        { can::focSetParameterId, "Setting" },
        { can::focQueryParameterId, "Setting Query" },
        { can::focCalibrateId, "Calibrate" },
        { can::focClearCalibrationId, "Clear Calibration" },
        { can::focClearDiagnosticsId, "Clear Diagnostics" },
        { can::focQueryFaultHistoryId, "Fault History Query" },
        { can::focClearFaultHistoryId, "Clear Fault History" },
        { can::focRecalibrateId, "Recalibrate" },
        { can::focResetModelId, "Reset Model" },
    };

    for (const auto& [commandType, name] : names)
        EXPECT_EQ(name, CommandDisplayName(can::focMotorCategoryId, commandType)) << static_cast<int>(commandType);

    EXPECT_TRUE(CommandDisplayName(can::focMotorCategoryId, 0xFF).empty());
}

TEST_F(TestCommandDescriptions, describes_every_acknowledgement)
{
    const std::pair<services::CanAckStatus, std::string_view> descriptions[]{
        { services::CanAckStatus::success, "accepted" },
        { services::CanAckStatus::unknownCommand, "rejected: the controller does not know this command" },
        { services::CanAckStatus::invalidPayload, "rejected: value out of range or malformed" },
        { services::CanAckStatus::invalidState, "rejected: not allowed in the motor's current state" },
        { services::CanAckStatus::sequenceError, "rejected: out of sequence" },
        { services::CanAckStatus::rateLimited, "rejected: sent too fast, retry shortly" },
        { services::CanAckStatus::notImplemented, "rejected: not implemented by this firmware" },
        { services::CanAckStatus::categoryError, "rejected by the motor controller" },
        { static_cast<services::CanAckStatus>(0xFF), "unknown acknowledgement" },
    };

    for (const auto& [status, description] : descriptions)
        EXPECT_EQ(description, AckStatusDescription(status)) << static_cast<int>(status);
}

TEST_F(TestCommandDescriptions, describes_every_rejection)
{
    const std::pair<can::FocMotorCategoryError, std::string_view> descriptions[]{
        { can::FocMotorCategoryError::busy, "rejected: the controller is busy, retry shortly" },
        { can::FocMotorCategoryError::persistenceFailed, "rejected: the value could not be stored" },
        { can::FocMotorCategoryError::modeMismatch, "rejected: not valid in the active control mode" },
        { can::FocMotorCategoryError::calibrationFailed, "rejected: calibration failed" },
        { can::FocMotorCategoryError::abortedByFault, "rejected: aborted by a motor fault" },
        { can::FocMotorCategoryError::applicationError, "rejected by the motor controller" },
        { can::FocMotorCategoryError::invalidParameters, "rejected: the controller cannot be designed from the motor parameters and settings it has" },
        { static_cast<can::FocMotorCategoryError>(0xFF), "rejected for an unknown reason" },
    };

    for (const auto& [error, description] : descriptions)
        EXPECT_EQ(description, RejectionDescription(error)) << static_cast<int>(error);
}
