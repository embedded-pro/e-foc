#include "core/can/FocMotorCanClient.hpp"

namespace can
{
    FocMotorCanClient::FocMotorCanClient(hal::Can& can, uint16_t targetNodeId)
        : nodeId{ targetNodeId }
        , transport{ can, targetNodeId }
        , protocolClient{ can }
        , categoryClient{ transport, protocolClient }
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

    services::CanProtocolClient& FocMotorCanClient::ProtocolClient()
    {
        return protocolClient;
    }

    FocMotorCategoryClient& FocMotorCanClient::CategoryClient()
    {
        return categoryClient;
    }
}
