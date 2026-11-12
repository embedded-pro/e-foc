#pragma once

#include "e-foc-protocol/client/FocMotorCategoryClient.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "tools/can_commander/logic/CommandTracker.hpp"
#include <cstdint>

namespace tool
{
    class MotorCommands
    {
    public:
        MotorCommands(can::FocMotorCategoryClient& category, CommandTracker& tracker);

        bool SendStartMotor(uint16_t nodeId);
        bool SendStopMotor(uint16_t nodeId);
        bool SendEmergencyStop(uint16_t nodeId);
        bool SendSetControlMode(uint16_t nodeId, can::FocMotorMode mode);
        bool SendSetTorqueSetpoint(uint16_t nodeId, float iqCurrent);
        bool SendSetSpeedSetpoint(uint16_t nodeId, float speedRadPerSec);
        bool SendSetPositionSetpoint(uint16_t nodeId, float positionRad);
        bool SendSetTorqueSetpointNewtonMeter(uint16_t nodeId, float torqueNewtonMeter);

        bool SendQuickStop(uint16_t nodeId);
        bool SendClearFault(uint16_t nodeId);
        bool SendAlign(uint16_t nodeId);
        bool SendIdentifyElectrical(uint16_t nodeId);
        bool SendCalibrate(uint16_t nodeId);
        bool SendClearCalibration(uint16_t nodeId);
        bool SendClearDiagnostics(uint16_t nodeId);
        bool SendClearFaultHistory(uint16_t nodeId);
        bool SendRecalibrate(uint16_t nodeId);
        bool SendResetModel(uint16_t nodeId);
        bool SendSelectAlgorithm(uint16_t nodeId, can::FocControlLoop loop, uint8_t algorithm);
        bool SendSetParameter(uint16_t nodeId, can::FocParameter parameter, float value);

    private:
        template<class Send>
        bool Tracked(uint16_t nodeId, uint8_t commandType, Send send);

        bool SendSetpoint(uint16_t nodeId, can::FocSetpointKind kind, float value);
        bool SendMaintenance(uint16_t nodeId, uint8_t commandType, can::FocMaintenanceCommand command);

        can::FocMotorCategoryClient& category;
        CommandTracker& tracker;
    };
}
