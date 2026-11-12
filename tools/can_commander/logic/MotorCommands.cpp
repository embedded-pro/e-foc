#include "tools/can_commander/logic/MotorCommands.hpp"

namespace tool
{
    MotorCommands::MotorCommands(can::FocMotorCategoryClient& category, CommandTracker& tracker)
        : category{ category }
        , tracker{ tracker }
    {}

    template<class Send>
    bool MotorCommands::Tracked(uint16_t nodeId, uint8_t commandType, Send send)
    {
        if (!send())
            return false;

        tracker.MarkBusy(nodeId, commandType);
        return true;
    }

    bool MotorCommands::SendStartMotor(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focStartId, [this, nodeId]()
            {
                return category.SendStart(nodeId);
            });
    }

    bool MotorCommands::SendStopMotor(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focStopId, [this, nodeId]()
            {
                return category.SendStop(nodeId);
            });
    }

    bool MotorCommands::SendEmergencyStop(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focEmergencyStopId, [this, nodeId]()
            {
                return category.SendEmergencyStop(nodeId);
            });
    }

    bool MotorCommands::SendQuickStop(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focQuickStopId, [this, nodeId]()
            {
                return category.SendQuickStop(nodeId);
            });
    }

    bool MotorCommands::SendSetControlMode(uint16_t nodeId, can::FocMotorMode mode)
    {
        return Tracked(nodeId, can::focSelectControlModeId, [this, nodeId, mode]()
            {
                return category.SendSelectControlMode(nodeId, mode);
            });
    }

    bool MotorCommands::SendSetpoint(uint16_t nodeId, can::FocSetpointKind kind, float value)
    {
        return Tracked(nodeId, can::SetpointCommandId(kind), [this, nodeId, kind, value]()
            {
                return category.SendSetpoint(nodeId, can::FocSetpoint{ kind, value });
            });
    }

    bool MotorCommands::SendSetTorqueSetpoint(uint16_t nodeId, float iqCurrent)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::torqueCurrent, iqCurrent);
    }

    bool MotorCommands::SendSetSpeedSetpoint(uint16_t nodeId, float speedRadPerSec)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::speed, speedRadPerSec);
    }

    bool MotorCommands::SendSetPositionSetpoint(uint16_t nodeId, float positionRad)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::position, positionRad);
    }

    bool MotorCommands::SendSetTorqueSetpointNewtonMeter(uint16_t nodeId, float torqueNewtonMeter)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::torqueNewtonMeter, torqueNewtonMeter);
    }

    bool MotorCommands::SendClearFault(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focClearFaultId, [this, nodeId]()
            {
                return category.SendClearFault(nodeId);
            });
    }

    bool MotorCommands::SendAlign(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focAlignId, [this, nodeId]()
            {
                return category.SendAlign(nodeId);
            });
    }

    bool MotorCommands::SendIdentifyElectrical(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focIdentifyElectricalId, [this, nodeId]()
            {
                return category.SendIdentifyElectrical(nodeId);
            });
    }

    bool MotorCommands::SendCalibrate(uint16_t nodeId)
    {
        return SendMaintenance(nodeId, can::focCalibrateId, can::FocMaintenanceCommand::calibrate);
    }

    bool MotorCommands::SendClearCalibration(uint16_t nodeId)
    {
        return SendMaintenance(nodeId, can::focClearCalibrationId, can::FocMaintenanceCommand::clearCalibration);
    }

    bool MotorCommands::SendClearDiagnostics(uint16_t nodeId)
    {
        return SendMaintenance(nodeId, can::focClearDiagnosticsId, can::FocMaintenanceCommand::clearDiagnostics);
    }

    bool MotorCommands::SendClearFaultHistory(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focClearFaultHistoryId, [this, nodeId]()
            {
                return category.SendClearFaultHistory(nodeId);
            });
    }

    bool MotorCommands::SendRecalibrate(uint16_t nodeId)
    {
        return SendMaintenance(nodeId, can::focRecalibrateId, can::FocMaintenanceCommand::recalibrate);
    }

    bool MotorCommands::SendResetModel(uint16_t nodeId)
    {
        return SendMaintenance(nodeId, can::focResetModelId, can::FocMaintenanceCommand::resetModel);
    }

    bool MotorCommands::SendSelectAlgorithm(uint16_t nodeId, can::FocControlLoop loop, uint8_t algorithm)
    {
        return Tracked(nodeId, can::focSelectAlgorithmId, [this, nodeId, loop, algorithm]()
            {
                return category.SendSelectAlgorithm(nodeId, loop, algorithm);
            });
    }

    bool MotorCommands::SendSetParameter(uint16_t nodeId, can::FocParameter parameter, float value)
    {
        return Tracked(nodeId, can::focSetParameterId, [this, nodeId, parameter, value]()
            {
                return category.SendSetParameter(nodeId, parameter, value);
            });
    }

    bool MotorCommands::SendMaintenance(uint16_t nodeId, uint8_t commandType, can::FocMaintenanceCommand command)
    {
        return Tracked(nodeId, commandType, [this, nodeId, command]()
            {
                return category.SendMaintenanceCommand(nodeId, command);
            });
    }
}
