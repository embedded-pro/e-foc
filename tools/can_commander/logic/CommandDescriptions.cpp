#include "tools/can_commander/logic/CommandDescriptions.hpp"
#include "core/can/FocMotorMessages.hpp"

namespace tool
{
    std::string_view CommandDisplayName(uint8_t categoryId, uint8_t commandType)
    {
        if (categoryId != can::focMotorCategoryId)
            return {};

        switch (commandType)
        {
            case can::focStartId:
                return "Start Motor";
            case can::focStopId:
                return "Stop Motor";
            case can::focEmergencyStopId:
                return "Emergency Stop";
            case can::focClearFaultId:
                return "Clear Fault";
            case can::focSelectControlModeId:
                return "Control Mode";
            case can::focSetTorqueSetpointId:
                return "Torque Setpoint";
            case can::focSetSpeedSetpointId:
                return "Speed Setpoint";
            case can::focSetPositionSetpointId:
                return "Position Setpoint";
            case can::focSetPidCurrentId:
                return "Current Loop Bandwidth";
            case can::focSetPidSpeedId:
                return "Speed Loop Bandwidth";
            case can::focSetPidPositionId:
                return "Position Loop Bandwidth";
            case can::focQueryContractVersionId:
                return "Firmware Compatibility Check";
            default:
                return {};
        }
    }

    std::string_view AckStatusDescription(services::CanAckStatus status)
    {
        switch (status)
        {
            case services::CanAckStatus::success:
                return "accepted";
            case services::CanAckStatus::unknownCommand:
                return "rejected: the controller does not know this command";
            case services::CanAckStatus::invalidPayload:
                return "rejected: value out of range or malformed";
            case services::CanAckStatus::invalidState:
                return "rejected: not allowed in the motor's current state";
            case services::CanAckStatus::sequenceError:
                return "rejected: out of sequence";
            case services::CanAckStatus::rateLimited:
                return "rejected: sent too fast, retry shortly";
            case services::CanAckStatus::notImplemented:
                return "rejected: not implemented by this firmware";
            case services::CanAckStatus::categoryError:
                return "rejected by the motor controller";
            default:
                return "unknown acknowledgement";
        }
    }

    std::string_view RejectionDescription(can::FocMotorCategoryError error)
    {
        switch (error)
        {
            case can::FocMotorCategoryError::busy:
                return "rejected: the controller is busy, retry shortly";
            case can::FocMotorCategoryError::persistenceFailed:
                return "rejected: the value could not be stored";
            case can::FocMotorCategoryError::modeMismatch:
                return "rejected: not valid in the active control mode";
            case can::FocMotorCategoryError::calibrationFailed:
                return "rejected: calibration failed";
            case can::FocMotorCategoryError::abortedByFault:
                return "rejected: aborted by a motor fault";
            case can::FocMotorCategoryError::applicationError:
                return "rejected by the motor controller";
            default:
                return "rejected for an unknown reason";
        }
    }
}
