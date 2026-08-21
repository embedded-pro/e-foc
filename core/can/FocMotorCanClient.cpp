#include "core/can/FocMotorCanClient.hpp"

namespace can
{
    FocMotorCanClient::FocMotorCanClient(hal::Can& can, uint16_t targetNodeId)
        : nodeId{ targetNodeId }
        , protocolClient{ can }
        , categoryClient{ protocolClient.Transport(), protocolClient }
    {
        protocolClient.RegisterCategory(categoryClient);
    }

    bool FocMotorCanClient::Start()
    {
        return categoryClient.SendStart(nodeId);
    }

    bool FocMotorCanClient::Stop()
    {
        return categoryClient.SendStop(nodeId);
    }

    bool FocMotorCanClient::ClearFault()
    {
        return categoryClient.SendClearFault(nodeId);
    }

    bool FocMotorCanClient::EmergencyStop()
    {
        return categoryClient.SendEmergencyStop(nodeId);
    }

    bool FocMotorCanClient::SelectControlMode(FocMotorMode mode)
    {
        return categoryClient.SendSelectControlMode(nodeId, mode);
    }

    bool FocMotorCanClient::SetTorque(foc::Ampere value)
    {
        return categoryClient.SendSetTorqueSetpoint(nodeId, value);
    }

    bool FocMotorCanClient::SetSpeed(foc::RadiansPerSecond value)
    {
        return categoryClient.SendSetSpeedSetpoint(nodeId, value);
    }

    bool FocMotorCanClient::SetPosition(foc::Radians value)
    {
        return categoryClient.SendSetPositionSetpoint(nodeId, value);
    }

    bool FocMotorCanClient::SetCurrentIdPid(float kp, float ki, float kd)
    {
        return categoryClient.SendSetCurrentIdPid(nodeId, kp, ki, kd);
    }

    bool FocMotorCanClient::SetCurrentIqPid(float kp, float ki, float kd)
    {
        return categoryClient.SendSetCurrentIqPid(nodeId, kp, ki, kd);
    }

    bool FocMotorCanClient::SetSpeedPid(float kp, float ki, float kd)
    {
        return categoryClient.SendSetSpeedPid(nodeId, kp, ki, kd);
    }

    bool FocMotorCanClient::SetPositionPid(float kp, float ki, float kd)
    {
        return categoryClient.SendSetPositionPid(nodeId, kp, ki, kd);
    }

    services::CanProtocolClient& FocMotorCanClient::ProtocolClient()
    {
        return protocolClient;
    }

    FocMotorCategoryClient& FocMotorCanClient::CategoryClient()
    {
        return categoryClient;
    }
}
