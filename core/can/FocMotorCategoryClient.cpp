#include "core/can/FocMotorCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "core/can/FocMotorWireContract.hpp"

namespace can
{
    FocMotorCategoryClient::FocMotorCategoryClient(services::CanFrameTransport& transport,
        services::CanSequenceSource& sequenceSource)
        : CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(selectControlModeResponse, categoryError, telemetryStatus, telemetryElectrical, contractVersionResponse);
    }

    uint8_t FocMotorCategoryClient::Id() const
    {
        return focMotorCategoryId;
    }

    bool FocMotorCategoryClient::SendStart(uint16_t targetNodeId)
    {
        if (SendsRefused())
            return false;

        return SendCommand(targetNodeId, focStartId);
    }

    bool FocMotorCategoryClient::SendStop(uint16_t targetNodeId)
    {
        if (SendsRefused())
            return false;

        return SendCommand(targetNodeId, focStopId);
    }

    bool FocMotorCategoryClient::SendClearFault(uint16_t targetNodeId)
    {
        if (SendsRefused())
            return false;

        return SendCommand(targetNodeId, focClearFaultId);
    }

    bool FocMotorCategoryClient::SendEmergencyStop(uint16_t targetNodeId)
    {
        if (SendsRefused())
            return false;

        return SendCommand(targetNodeId, focEmergencyStopId, services::CanPriority::emergency);
    }

    bool FocMotorCategoryClient::SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(mode));
        return SendCommand(targetNodeId, focSelectControlModeId, payload);
    }

    bool FocMotorCategoryClient::SendSetTorqueSetpoint(uint16_t targetNodeId, foc::Ampere value)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focCurrentScale);
        return SendCommand(targetNodeId, focSetTorqueSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetSpeedSetpoint(uint16_t targetNodeId, foc::RadiansPerSecond value)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focSpeedScale);
        return SendCommand(targetNodeId, focSetSpeedSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetPositionSetpoint(uint16_t targetNodeId, foc::Radians value)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focPositionScale);
        return SendCommand(targetNodeId, focSetPositionSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetCurrentBandwidth(uint16_t targetNodeId, float bandwidth)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(bandwidth, focPidScale);
        return SendCommand(targetNodeId, focSetPidCurrentId, payload);
    }

    bool FocMotorCategoryClient::SendSetSpeedBandwidth(uint16_t targetNodeId, float bandwidth)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(bandwidth, focPidScale);
        return SendCommand(targetNodeId, focSetPidSpeedId, payload);
    }

    bool FocMotorCategoryClient::SendSetPositionBandwidth(uint16_t targetNodeId, float bandwidth)
    {
        if (SendsRefused())
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(bandwidth, focPidScale);
        return SendCommand(targetNodeId, focSetPidPositionId, payload);
    }

    bool FocMotorCategoryClient::SendQueryContractVersion(uint16_t targetNodeId)
    {
        services::CanPayloadWriter payload;
        return SendCommand(targetNodeId, focQueryContractVersionId, payload);
    }

    ContractCompatibility FocMotorCategoryClient::Compatibility() const
    {
        return contractCompatibility;
    }

    bool FocMotorCategoryClient::SendsRefused() const
    {
        return contractCompatibility == ContractCompatibility::incompatible;
    }

    bool FocMotorCategoryClient::HandleContractVersionResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focContractVersionResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto major = reader.ReadUInt8();
        const auto minor = reader.ReadUInt8();

        contractCompatibility = major == focContractVersionMajor ? ContractCompatibility::compatible : ContractCompatibility::incompatible;

        NotifyObservers([major, minor](auto& observer)
            {
                observer.OnContractVersionResponse(major, minor);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleSelectControlModeResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSelectControlModeResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto activeMode = static_cast<FocMotorMode>(reader.ReadUInt8());
        NotifyObservers([activeMode](auto& observer)
            {
                observer.OnSelectControlModeResponse(activeMode);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleCategoryError(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, services::canCategoryErrorResponseMessageTypeId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto originCommandId = reader.ReadUInt8();
        const auto errorCode = static_cast<FocMotorCategoryError>(reader.ReadUInt8());
        NotifyObservers([originCommandId, errorCode](auto& observer)
            {
                observer.OnCategoryError(originCommandId, errorCode);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleTelemetryStatus(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focTelemetryStatusResponseId))
            return false;

        NotifyObservers([&data](auto& observer)
            {
                observer.OnTelemetryStatus(data);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleTelemetryElectrical(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focTelemetryElectricalResponseId))
            return false;

        NotifyObservers([&data](auto& observer)
            {
                observer.OnTelemetryElectrical(data);
            });

        return true;
    }
}
