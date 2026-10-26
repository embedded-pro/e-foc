#include "core/can/FocMotorCategoryClient.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <cstddef>

namespace can
{
    FocMotorCategoryClient::FocMotorCategoryClient(services::CanFrameTransport& transport,
        services::CanSequenceSource& sequenceSource, const CanFrameOrigin& origin)
        : CanCategoryClient(transport, sequenceSource)
        , origin(origin)
    {
        AddMessageTypes(selectControlModeResponse, categoryError, telemetryStatus, telemetryElectrical, contractVersionResponse,
            electricalParamsResponse, mechanicalParamsResponse, algorithmsResponse, parameterResponse);
    }

    uint8_t FocMotorCategoryClient::Id() const
    {
        return focMotorCategoryId;
    }

    bool FocMotorCategoryClient::SendStart(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focStartId);
    }

    bool FocMotorCategoryClient::SendStop(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focStopId);
    }

    bool FocMotorCategoryClient::SendClearFault(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focClearFaultId);
    }

    bool FocMotorCategoryClient::SendEmergencyStop(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focEmergencyStopId, services::CanPriority::emergency);
    }

    bool FocMotorCategoryClient::SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(mode));
        return SendCommand(targetNodeId, focSelectControlModeId, payload);
    }

    bool FocMotorCategoryClient::SendSetTorqueSetpoint(uint16_t targetNodeId, foc::Ampere value)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focCurrentScale);
        return SendCommand(targetNodeId, focSetTorqueSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetSpeedSetpoint(uint16_t targetNodeId, foc::RadiansPerSecond value)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focSpeedScale);
        return SendCommand(targetNodeId, focSetSpeedSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetPositionSetpoint(uint16_t targetNodeId, foc::Radians value)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(value.Value(), focPositionScale);
        return SendCommand(targetNodeId, focSetPositionSetpointId, payload);
    }

    bool FocMotorCategoryClient::SendSetCurrentBandwidth(uint16_t targetNodeId, float bandwidth)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(bandwidth, focPidScale);
        return SendCommand(targetNodeId, focSetPidCurrentId, payload);
    }

    bool FocMotorCategoryClient::SendSetSpeedBandwidth(uint16_t targetNodeId, float bandwidth)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteFixed16(bandwidth, focPidScale);
        return SendCommand(targetNodeId, focSetPidSpeedId, payload);
    }

    bool FocMotorCategoryClient::SendSetPositionBandwidth(uint16_t targetNodeId, float bandwidth)
    {
        if (contracts.Incompatible(targetNodeId))
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

    bool FocMotorCategoryClient::SendMaintenanceCommand(uint16_t targetNodeId, FocMaintenanceCommand command)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, static_cast<uint8_t>(command));
    }

    bool FocMotorCategoryClient::SendAlign(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focAlignId);
    }

    bool FocMotorCategoryClient::SendIdentifyElectrical(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focIdentifyElectricalId);
    }

    bool FocMotorCategoryClient::SendIdentifyMechanical(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focIdentifyMechanicalId);
    }

    bool FocMotorCategoryClient::SendRequestTelemetry(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focRequestTelemetryId);
    }

    bool FocMotorCategoryClient::SendSetEncoderResolution(uint16_t targetNodeId, uint32_t countsPerRevolution)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt32(countsPerRevolution);
        return SendCommand(targetNodeId, focSetEncoderResolutionId, payload);
    }

    bool FocMotorCategoryClient::SendSelectAlgorithm(uint16_t targetNodeId, FocControlLoop loop, uint8_t algorithm)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(loop));
        payload.WriteUInt8(algorithm);
        return SendCommand(targetNodeId, focSelectAlgorithmId, payload);
    }

    bool FocMotorCategoryClient::SendQueryAlgorithms(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focQueryAlgorithmsId);
    }

    bool FocMotorCategoryClient::SendSetParameter(uint16_t targetNodeId, FocParameter parameter, float value)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(parameter));
        WriteFloat32(payload, value);
        return SendCommand(targetNodeId, focSetParameterId, payload);
    }

    bool FocMotorCategoryClient::SendQueryParameter(uint16_t targetNodeId, FocParameter parameter)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(parameter));
        return SendCommand(targetNodeId, focQueryParameterId, payload);
    }

    ContractCompatibility NodeContracts::Of(uint16_t nodeId) const
    {
        for (const auto& contract : contracts)
            if (contract.nodeId == nodeId && nodeId != 0)
                return contract.compatibility;

        return ContractCompatibility::unknown;
    }

    bool NodeContracts::Incompatible(uint16_t nodeId) const
    {
        return Of(nodeId) == ContractCompatibility::incompatible;
    }

    void NodeContracts::Record(uint16_t nodeId, ContractCompatibility compatibility)
    {
        for (auto& contract : contracts)
            if (contract.nodeId == nodeId)
            {
                contract.compatibility = compatibility;
                return;
            }

        for (auto& contract : contracts)
            if (contract.nodeId == 0)
            {
                contract = NodeContract{ nodeId, compatibility };
                return;
            }

        contracts[nextEvicted] = NodeContract{ nodeId, compatibility };
        nextEvicted = (nextEvicted + 1) % contracts.size();
    }

    ContractCompatibility FocMotorCategoryClient::Compatibility(uint16_t nodeId) const
    {
        return contracts.Of(nodeId);
    }

    bool FocMotorCategoryClient::HandleContractVersionResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focContractVersionResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto major = reader.ReadUInt8();
        const auto minor = reader.ReadUInt8();

        const auto source = origin.SourceNodeId();
        if (source != 0)
            contracts.Record(source, major == focContractVersionMajor ? ContractCompatibility::compatible : ContractCompatibility::incompatible);

        NotifyObservers([source, major, minor](auto& observer)
            {
                observer.OnContractVersionResponse(source, major, minor);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleSelectControlModeResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSelectControlModeResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto activeMode = static_cast<FocMotorMode>(reader.ReadUInt8());
        NotifyObservers([source = origin.SourceNodeId(), activeMode](auto& observer)
            {
                observer.OnSelectControlModeResponse(source, activeMode);
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
        NotifyObservers([source = origin.SourceNodeId(), originCommandId, errorCode](auto& observer)
            {
                observer.OnCategoryError(source, originCommandId, errorCode);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleTelemetryStatus(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focTelemetryStatusResponseId))
            return false;

        NotifyObservers([source = origin.SourceNodeId(), &data](auto& observer)
            {
                observer.OnTelemetryStatus(source, data);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleTelemetryElectrical(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focTelemetryElectricalResponseId))
            return false;

        NotifyObservers([source = origin.SourceNodeId(), &data](auto& observer)
            {
                observer.OnTelemetryElectrical(source, data);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleElectricalParamsResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focElectricalParamsResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto resistance = foc::Ohm{ reader.ReadFixed16(focResistanceScale) };
        const auto inductance = foc::MilliHenry{ reader.ReadFixed16(focInductanceScale) };
        const auto polePairs = reader.ReadUInt8();

        NotifyObservers([source = origin.SourceNodeId(), resistance, inductance, polePairs](auto& observer)
            {
                observer.OnElectricalParamsResponse(source, resistance, inductance, polePairs);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleAlgorithmsResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focAlgorithmsResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto mode = reader.ReadUInt8();
        const auto current = reader.ReadUInt8();
        const auto speed = reader.ReadUInt8();
        const auto position = reader.ReadUInt8();

        if (mode > static_cast<uint8_t>(FocMotorMode::position) || current >= AlgorithmCount(FocControlLoop::current) ||
            speed >= AlgorithmCount(FocControlLoop::speed) || position >= AlgorithmCount(FocControlLoop::position))
            return false;

        const FocAlgorithms algorithms{ static_cast<FocMotorMode>(mode), static_cast<FocCurrentAlgorithm>(current),
            static_cast<FocSpeedAlgorithm>(speed), static_cast<FocPositionAlgorithm>(position) };

        NotifyObservers([source = origin.SourceNodeId(), &algorithms](auto& observer)
            {
                observer.OnAlgorithmsResponse(source, algorithms);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleParameterResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focParameterResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto descriptor = FindParameter(reader.ReadUInt8());
        const auto flags = std::byte{ reader.ReadUInt8() };
        const auto value = ReadFloat32(reader);

        if (!descriptor.has_value())
            return false;

        const FocParameterValue held{ value, (flags & std::byte{ focParameterAutomaticFlag }) != std::byte{} };

        NotifyObservers([source = origin.SourceNodeId(), parameter = descriptor->id, held](auto& observer)
            {
                observer.OnParameterResponse(source, parameter, held);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleMechanicalParamsResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focMechanicalParamsResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto friction = foc::NewtonMeterSecondPerRadian{ static_cast<float>(reader.ReadUInt32()) / static_cast<float>(focFrictionScale) };
        const auto inertia = foc::NewtonMeterSecondSquared{ static_cast<float>(reader.ReadUInt32()) / static_cast<float>(focInertiaScale) };

        NotifyObservers([source = origin.SourceNodeId(), friction, inertia](auto& observer)
            {
                observer.OnMechanicalParamsResponse(source, friction, inertia);
            });

        return true;
    }
}
