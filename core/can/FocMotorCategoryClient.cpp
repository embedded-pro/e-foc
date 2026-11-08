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
        AddMessageTypes(selectControlModeResponse, categoryError, telemetryStatus, telemetryMotion, telemetryCurrents, contractVersionResponse,
            algorithmsResponse, parameterResponse);
    }

    FocMotorCategoryClient::FaultHistoryMessages::FaultHistoryMessages(FocMotorCategoryClient& client)
        : client(client)
        , faultRecordResponse{ focFaultRecordResponseId, *this, &FaultHistoryMessages::HandleFaultRecordResponse }
        , faultConditionsResponse{ focFaultConditionsResponseId, *this, &FaultHistoryMessages::HandleFaultConditionsResponse }
    {
        client.AddMessageTypes(faultRecordResponse, faultConditionsResponse);
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

    bool FocMotorCategoryClient::SendSetpoint(uint16_t targetNodeId, const FocSetpoint& setpoint)
    {
        if (contracts.Incompatible(targetNodeId) || !IsFiniteWireValue(setpoint.value))
            return false;

        services::CanPayloadWriter payload;
        WriteFloat32(payload, setpoint.value);
        return SendCommand(targetNodeId, SetpointCommandId(setpoint.kind), payload);
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

    bool FocMotorCategoryClient::SendRequestTelemetry(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focRequestTelemetryId);
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
        if (contracts.Incompatible(targetNodeId) && !IsFirmwareIdentity(parameter))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(parameter));
        return SendCommand(targetNodeId, focQueryParameterId, payload);
    }

    bool FocMotorCategoryClient::SendQueryFaultHistory(uint16_t targetNodeId, uint8_t age)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(age);
        return SendCommand(targetNodeId, focQueryFaultHistoryId, payload);
    }

    bool FocMotorCategoryClient::SendClearFaultHistory(uint16_t targetNodeId)
    {
        if (contracts.Incompatible(targetNodeId))
            return false;

        return SendCommand(targetNodeId, focClearFaultHistoryId);
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
            contracts.Record(source, IsCompatibleContract(major, minor) ? ContractCompatibility::compatible : ContractCompatibility::incompatible);

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

        services::CanPayloadReader reader{ data };
        const auto state = reader.ReadUInt8();
        const auto fault = static_cast<FocFaultCode>(reader.ReadUInt8());

        if (state > static_cast<uint8_t>(FocMotorState::ready))
            return false;

        NotifyObservers([source = origin.SourceNodeId(), state = static_cast<FocMotorState>(state), fault](auto& observer)
            {
                observer.OnTelemetryStatus(source, state, fault);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleTelemetryMotion(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focTelemetryMotionId))
            return false;

        services::CanPayloadReader reader{ data };
        const foc::RadiansPerSecond speed{ ReadFloat32(reader) };
        const foc::Radians position{ ReadFloat32(reader) };

        if (!IsFiniteWireValue(speed.Value()) || !IsFiniteWireValue(position.Value()))
            return false;

        NotifyObservers([source = origin.SourceNodeId(), speed, position](auto& observer)
            {
                observer.OnTelemetryMotion(source, speed, position);
            });

        return true;
    }

    bool FocMotorCategoryClient::HandleTelemetryCurrents(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focTelemetryCurrentsId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto sequence = reader.ReadUInt8();
        const auto id = foc::Ampere{ reader.ReadFixed16(focStreamCurrentScale) };
        const auto iq = foc::Ampere{ reader.ReadFixed16(focStreamCurrentScale) };
        const auto position = foc::Radians{ reader.ReadFixed16(focStreamPositionScale) };

        NotifyObservers([source = origin.SourceNodeId(), sequence, id, iq, position](auto& observer)
            {
                observer.OnTelemetryCurrents(source, sequence, id, iq, position);
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

    bool FocMotorCategoryClient::FaultHistoryMessages::HandleFaultRecordResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focFaultRecordResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto age = reader.ReadUInt8();
        const auto code = static_cast<FocFaultCode>(reader.ReadUInt8());
        const auto mode = reader.ReadUInt8();
        const auto uptimeMs = reader.ReadUInt32();

        if (mode > static_cast<uint8_t>(FocMotorMode::position))
            return false;

        client.NotifyObservers([source = client.origin.SourceNodeId(), age, code, mode = static_cast<FocMotorMode>(mode), uptimeMs](auto& observer)
            {
                observer.OnFaultRecordResponse(source, age, code, mode, uptimeMs);
            });

        return true;
    }

    bool FocMotorCategoryClient::FaultHistoryMessages::HandleFaultConditionsResponse(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focFaultConditionsResponseId))
            return false;

        services::CanPayloadReader reader{ data };
        const auto age = reader.ReadUInt8();
        const auto powerUp = reader.ReadInt16();
        const foc::Volts busVoltage{ reader.ReadFixed16(focRecordedVoltageScale) };
        const foc::Ampere peakCurrent{ reader.ReadFixed16(focRecordedCurrentScale) };

        client.NotifyObservers([source = client.origin.SourceNodeId(), age, powerUp, busVoltage, peakCurrent](auto& observer)
            {
                observer.OnFaultConditionsResponse(source, age, powerUp, busVoltage, peakCurrent);
            });

        return true;
    }
}
