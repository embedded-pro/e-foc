#include "core/can/FocMotorCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace can
{
    FocMotorCategoryClient::FocMotorCategoryClient(services::CanFrameTransport& transport,
        services::CanSequenceSource& sequenceSource)
        : CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(selectControlModeResponse, categoryError, telemetryStatus, telemetryElectrical);
    }

    uint8_t FocMotorCategoryClient::Id() const
    {
        return focMotorCategoryId;
    }

    bool FocMotorCategoryClient::SendStart(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focStartId);
    }

    bool FocMotorCategoryClient::SendStop(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focStopId);
    }

    bool FocMotorCategoryClient::SendClearFault(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focClearFaultId);
    }

    bool FocMotorCategoryClient::SendEmergencyStop(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, focEmergencyStopId, services::CanPriority::emergency);
    }

    bool FocMotorCategoryClient::SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(mode));
        return SendCommand(targetNodeId, focSelectControlModeId, payload);
    }

    bool FocMotorCategoryClient::SendSetTorqueSetpoint(uint16_t targetNodeId, foc::Ampere value)
    {
        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focCurrentScale);
        return SendCommand(targetNodeId, focSetTorqueSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetSpeedSetpoint(uint16_t targetNodeId, foc::RadiansPerSecond value)
    {
        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focSpeedScale);
        return SendCommand(targetNodeId, focSetSpeedSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetPositionSetpoint(uint16_t targetNodeId, foc::Radians value)
    {
        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focPositionScale);
        return SendCommand(targetNodeId, focSetPositionSetpointId, payload);
    }

    void FocMotorCategoryClient::HandleSelectControlModeResponse(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        const auto activeMode = static_cast<FocMotorMode>(reader.ReadUInt8());
        if (!reader.Valid())
            return;
        NotifyObservers([activeMode](auto& observer)
            {
                observer.OnSelectControlModeResponse(activeMode);
            });
    }

    void FocMotorCategoryClient::HandleCategoryError(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        const auto originCommandId = reader.ReadUInt8();
        const auto errorCode = static_cast<FocMotorCategoryError>(reader.ReadUInt8());
        if (!reader.Valid())
            return;
        NotifyObservers([originCommandId, errorCode](auto& observer)
            {
                observer.OnCategoryError(originCommandId, errorCode);
            });
    }

    void FocMotorCategoryClient::HandleTelemetryStatus(const hal::Can::Message& data)
    {
        NotifyObservers([&data](auto& observer)
            {
                observer.OnTelemetryStatus(data);
            });
    }

    void FocMotorCategoryClient::HandleTelemetryElectrical(const hal::Can::Message& data)
    {
        NotifyObservers([&data](auto& observer)
            {
                observer.OnTelemetryElectrical(data);
            });
    }
}
