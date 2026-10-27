#include "tools/can_commander/logic/RecoveryAdvice.hpp"

namespace tool
{
    namespace
    {
        using can::FocMotorState;

        std::optional<std::string_view> SetpointModeName(uint8_t commandType)
        {
            switch (commandType)
            {
                case can::focSetTorqueSetpointId:
                    return "torque";
                case can::focSetSpeedSetpointId:
                    return "speed";
                case can::focSetPositionSetpointId:
                    return "position";
                default:
                    return std::nullopt;
            }
        }

        bool MovesOrHoldsTheMotor(uint8_t commandType)
        {
            return commandType == can::focStartId || commandType == can::focAlignId || SetpointModeName(commandType).has_value();
        }

        bool IsOperatingCommand(uint8_t commandType)
        {
            return MovesOrHoldsTheMotor(commandType) || commandType == can::focStopId || commandType == can::focClearFaultId;
        }

        std::string ModeName(std::optional<can::FocMotorMode> mode)
        {
            switch (mode.value_or(static_cast<can::FocMotorMode>(0xff)))
            {
                case can::FocMotorMode::torque:
                    return "torque mode";
                case can::FocMotorMode::speed:
                    return "speed mode";
                case can::FocMotorMode::position:
                    return "position mode";
                default:
                    return "the active control mode";
            }
        }

        RecoveryAdvice AdviseFault(const RefusedCommand& refused)
        {
            if (refused.commandType == can::focClearFaultId)
                return RecoveryAdvice{ "The controller refused to clear the fault: it was already cleared three times without a clean run in between. "
                                       "Find and remove the cause, then restart the controller.",
                    RecoveryAction::none };

            const auto fault = refused.faultName.empty() ? std::string{} : " (" + std::string{ refused.faultName } + ")";
            return RecoveryAdvice{ "The motor is in fault" + fault + ". Clear the fault, then start the motor again.", RecoveryAction::clearFault };
        }

        std::optional<RecoveryAdvice> AdviseInvalidState(const RefusedCommand& refused)
        {
            if (!IsOperatingCommand(refused.commandType))
                return std::nullopt;

            switch (refused.state.value_or(FocMotorState::idle))
            {
                case FocMotorState::fault:
                    return AdviseFault(refused);
                case FocMotorState::calibrating:
                    return RecoveryAdvice{ "The motor is calibrating. Wait until it finishes; the motor status shows when it is done.", RecoveryAction::none };
                case FocMotorState::partialCalibration:
                    return RecoveryAdvice{ "The calibration is incomplete for " + ModeName(refused.mode) +
                                               ": speed and position mode need the inertia and the friction. Identify them or enter them in Motor Parameters, "
                                               "then align the rotor and start the motor.",
                        RecoveryAction::showMotorParameters };
                case FocMotorState::running:
                    return std::nullopt;
                default:
                    break;
            }

            if (!MovesOrHoldsTheMotor(refused.commandType) || refused.commandType == can::focAlignId)
                return std::nullopt;

            return RecoveryAdvice{ "The motor is not ready to run: its rotor has not been aligned since the controller started, or its pole pairs changed. "
                                   "Align the rotor (it moves briefly), then start the motor; setpoints are accepted once the rotor is aligned. "
                                   "If Start Motor is still refused after aligning, check the supply: the board protection may be active.",
                RecoveryAction::alignRotor };
        }

        std::optional<RecoveryAdvice> AdviseModeMismatch(const RefusedCommand& refused)
        {
            if (const auto setpointMode = SetpointModeName(refused.commandType))
                return RecoveryAdvice{ "A " + std::string{ *setpointMode } + " setpoint applies in " + std::string{ *setpointMode } + " mode only, and the controller runs " +
                                           ModeName(refused.mode) + ". Select " + std::string{ *setpointMode } + " mode while the motor is stopped, then apply the setpoint again.",
                    RecoveryAction::none };

            if (refused.commandType != can::focAlignId)
                return std::nullopt;

            switch (refused.state.value_or(FocMotorState::idle))
            {
                case FocMotorState::running:
                    return RecoveryAdvice{ "The rotor can only be aligned while the motor is stopped. Stop the motor, then align the rotor.", RecoveryAction::stopMotor };
                case FocMotorState::fault:
                case FocMotorState::calibrating:
                    return AdviseInvalidState(refused);
                default:
                    break;
            }

            return RecoveryAdvice{ "The controller holds no complete calibration for " + ModeName(refused.mode) +
                                       ", so it cannot align the rotor. Run Calibrate in Motor Parameters (the rotor will move), or enter the motor parameters there: "
                                       "speed and position mode also need the inertia and the friction.",
                RecoveryAction::showMotorParameters };
        }
    }

    std::optional<RecoveryAdvice> AdviseRecovery(const RefusedCommand& refused)
    {
        if (refused.refusal == Refusal::modeMismatch)
            return AdviseModeMismatch(refused);
        return AdviseInvalidState(refused);
    }

    std::string_view RecoveryActionName(RecoveryAction action)
    {
        switch (action)
        {
            case RecoveryAction::alignRotor:
                return "Align Rotor";
            case RecoveryAction::clearFault:
                return "Clear Fault";
            case RecoveryAction::stopMotor:
                return "Stop Motor";
            case RecoveryAction::showMotorParameters:
                return "Open Motor Parameters";
            default:
                return {};
        }
    }
}
