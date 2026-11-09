#include "core/can/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "core/can/FocMotorWireContract.hpp"

namespace can
{
    FocMotorCategoryServer::FocMotorCategoryServer(services::CanFrameTransport& transport)
        : CanCategoryServer(transport)
    {
        AddMessageTypes(start, stop, clearFault, emergencyStop, selectControlMode, requestTelemetry, queryContractVersion,
            settingsMessages.selectAlgorithm, settingsMessages.queryAlgorithms, settingsMessages.setParameter, settingsMessages.queryParameter);
    }

    FocMotorCategoryServer::CommissioningMessages::CommissioningMessages(FocMotorCategoryServer& server)
        : server(server)
        , align{ focAlignId, *this, &CommissioningMessages::HandleAlign }
        , identifyElectrical{ focIdentifyElectricalId, *this, &CommissioningMessages::HandleIdentifyElectrical }
        , calibrate{ focCalibrateId, *this, &CommissioningMessages::HandleCalibrate }
        , clearCalibration{ focClearCalibrationId, *this, &CommissioningMessages::HandleClearCalibration }
    {
        server.AddMessageTypes(align, identifyElectrical, calibrate, clearCalibration);
    }

    FocMotorCategoryServer::MaintenanceMessages::MaintenanceMessages(FocMotorCategoryServer& server)
        : server(server)
        , clearDiagnostics{ focClearDiagnosticsId, *this, &MaintenanceMessages::HandleClearDiagnostics }
        , queryFaultHistory{ focQueryFaultHistoryId, *this, &MaintenanceMessages::HandleQueryFaultHistory }
        , clearFaultHistory{ focClearFaultHistoryId, *this, &MaintenanceMessages::HandleClearFaultHistory }
        , recalibrate{ focRecalibrateId, *this, &MaintenanceMessages::HandleRecalibrate }
        , resetModel{ focResetModelId, *this, &MaintenanceMessages::HandleResetModel }
    {
        server.AddMessageTypes(clearDiagnostics, queryFaultHistory, clearFaultHistory, recalibrate, resetModel);
    }

    FocMotorCategoryServer::SettingsMessages::SettingsMessages(FocMotorCategoryServer& server)
        : selectAlgorithm{ focSelectAlgorithmId, server, &FocMotorCategoryServer::HandleSelectAlgorithm }
        , queryAlgorithms{ focQueryAlgorithmsId, server, &FocMotorCategoryServer::HandleQueryAlgorithms }
        , setParameter{ focSetParameterId, server, &FocMotorCategoryServer::HandleSetParameter }
        , queryParameter{ focQueryParameterId, server, &FocMotorCategoryServer::HandleQueryParameter }
    {}

    FocMotorCategoryServer::SetpointMessages::SetpointMessages(FocMotorCategoryServer& server)
        : server(server)
        , torqueCurrent{ focSetTorqueSetpointId, *this, &SetpointMessages::HandleTorqueCurrent }
        , torqueNewtonMeter{ focSetTorqueSetpointNewtonMeterId, *this, &SetpointMessages::HandleTorqueNewtonMeter }
        , speed{ focSetSpeedSetpointId, *this, &SetpointMessages::HandleSpeed }
        , position{ focSetPositionSetpointId, *this, &SetpointMessages::HandlePosition }
    {
        server.AddMessageTypes(torqueCurrent, torqueNewtonMeter, speed, position);
    }

    uint8_t FocMotorCategoryServer::Id() const
    {
        return focMotorCategoryId;
    }

    void FocMotorCategoryServer::SendSelectControlModeResponse(FocMotorMode activeMode)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(activeMode));
        SendResponse(focSelectControlModeResponseId, payload);
    }

    void FocMotorCategoryServer::SendCategoryError(uint8_t origCommandId, FocMotorCategoryError errorCode)
    {
        CanCategoryServer::SendCategoryError(origCommandId, static_cast<uint8_t>(errorCode));
        SendCommandAck(origCommandId, services::CanAckStatus::categoryError);
    }

    void FocMotorCategoryServer::BroadcastFaultStatus(FocFaultCode fault)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(FocMotorState::fault));
        payload.WriteUInt8(static_cast<uint8_t>(fault));

        SendEmergency(focTelemetryStatusResponseId, payload);
    }

    bool FocMotorCategoryServer::HandleStart(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focStartId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnStart([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focStartId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleStop(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focStopId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnStop([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focStopId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleClearFault(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focClearFaultId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnClearFault([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focClearFaultId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleEmergencyStop(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focEmergencyStopId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnEmergencyStop([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focEmergencyStopId, status);
                    });
            });

        return true;
    }

    void FocMotorCategoryServer::SendFaultRecord(const FocFaultRecord& record)
    {
        services::CanPayloadWriter identity;
        identity.WriteUInt8(record.age);
        identity.WriteUInt8(static_cast<uint8_t>(record.code));
        identity.WriteUInt8(static_cast<uint8_t>(record.mode));
        identity.WriteUInt32(record.uptimeMs);
        SendResponse(focFaultRecordResponseId, identity);

        services::CanPayloadWriter conditions;
        conditions.WriteUInt8(record.age);
        conditions.WriteInt16(record.powerUp);
        conditions.WriteFixed16(record.busVoltage.Value(), focRecordedVoltageScale);
        conditions.WriteFixed16(record.peakCurrent.Value(), focRecordedCurrentScale);
        SendResponse(focFaultConditionsResponseId, conditions);
    }

    bool FocMotorCategoryServer::HandleSelectControlMode(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSelectControlModeId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        auto rawMode = reader.ReadUInt8();

        if (!reader.Valid())
            return false;

        if (rawMode > static_cast<uint8_t>(FocMotorMode::position))
            return false;

        NotifyObservers([this, rawMode](auto& observer)
            {
                observer.OnSelectControlMode(static_cast<FocMotorMode>(rawMode), [this](FocMotorMode activatedMode)
                    {
                        SendSelectControlModeResponse(activatedMode);
                        SendCommandAck(focSelectControlModeId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::CommissioningMessages::HandleAlign(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focAlignId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnAlign([this](services::CanAckStatus status)
                    {
                        server.SendCommandAck(focAlignId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::CommissioningMessages::HandleIdentifyElectrical(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focIdentifyElectricalId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyElectrical([this]()
                    {
                        server.SendCommandAck(focIdentifyElectricalId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::CommissioningMessages::HandleCalibrate(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focCalibrateId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnCalibrate([this](services::CanAckStatus status)
                    {
                        server.SendCommandAck(focCalibrateId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::CommissioningMessages::HandleClearCalibration(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focClearCalibrationId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnClearCalibration([this](services::CanAckStatus status)
                    {
                        server.SendCommandAck(focClearCalibrationId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleRequestTelemetry(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focRequestTelemetryId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnRequestTelemetry([this]()
                    {
                        SendCommandAck(focRequestTelemetryId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleQueryContractVersion(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focQueryContractVersionId))
            return false;

        services::CanPayloadWriter payload;
        payload.WriteUInt8(focContractVersionMajor);
        payload.WriteUInt8(focContractVersionMinor);
        SendResponse(focContractVersionResponseId, payload);
        SendCommandAck(focQueryContractVersionId, services::CanAckStatus::success);

        return true;
    }

    bool FocMotorCategoryServer::HandleSelectAlgorithm(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSelectAlgorithmId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto rawLoop = reader.ReadUInt8();
        const auto algorithm = reader.ReadUInt8();

        if (!reader.Valid() || rawLoop > static_cast<uint8_t>(FocControlLoop::position))
            return false;

        const auto loop = static_cast<FocControlLoop>(rawLoop);
        if (algorithm >= AlgorithmCount(loop))
            return false;

        NotifyObservers([this, loop, algorithm](auto& observer)
            {
                observer.OnSelectAlgorithm(loop, algorithm, [this](const FocAlgorithms& algorithms)
                    {
                        SendAlgorithmsResponse(algorithms);
                        SendCommandAck(focSelectAlgorithmId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleQueryAlgorithms(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focQueryAlgorithmsId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnQueryAlgorithms([this](const FocAlgorithms& algorithms)
                    {
                        SendAlgorithmsResponse(algorithms);
                        SendCommandAck(focQueryAlgorithmsId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleSetParameter(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetParameterId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto descriptor = FindParameter(reader.ReadUInt8());
        const auto value = reader.ReadFloat32();

        if (!reader.Valid() || !descriptor.has_value() || !IsAcceptableParameterValue(*descriptor, value))
            return false;

        NotifyObservers([this, parameter = descriptor->id, value](auto& observer)
            {
                observer.OnSetParameter(parameter, value, [this, parameter](FocParameterValue held)
                    {
                        SendParameterResponse(parameter, held);
                        SendCommandAck(focSetParameterId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleQueryParameter(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focQueryParameterId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto descriptor = FindParameter(reader.ReadUInt8());

        if (!reader.Valid() || !descriptor.has_value())
            return false;

        NotifyObservers([this, parameter = descriptor->id](auto& observer)
            {
                observer.OnQueryParameter(parameter, [this, parameter](FocParameterValue held)
                    {
                        SendParameterResponse(parameter, held);
                        SendCommandAck(focQueryParameterId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    void FocMotorCategoryServer::SendAlgorithmsResponse(const FocAlgorithms& algorithms)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(algorithms.mode));
        payload.WriteUInt8(static_cast<uint8_t>(algorithms.current));
        payload.WriteUInt8(static_cast<uint8_t>(algorithms.speed));
        payload.WriteUInt8(static_cast<uint8_t>(algorithms.position));
        SendResponse(focAlgorithmsResponseId, payload);
    }

    void FocMotorCategoryServer::SendParameterResponse(FocParameter parameter, FocParameterValue value)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(parameter));
        payload.WriteUInt8(value.automatic ? focParameterAutomaticFlag : 0);
        payload.WriteFloat32(value.value);
        SendResponse(focParameterResponseId, payload);
    }

    void FocMotorCategoryServer::BroadcastTelemetryStatus(FocMotorState state, FocFaultCode fault)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(state));
        payload.WriteUInt8(static_cast<uint8_t>(fault));
        SendTelemetry(focTelemetryStatusResponseId, payload);
    }

    void FocMotorCategoryServer::BroadcastTelemetryMotion(foc::RadiansPerSecond speed, foc::Radians position)
    {
        services::CanPayloadWriter payload;
        payload.WriteFloat32(speed.Value());
        payload.WriteFloat32(position.Value());
        SendTelemetry(focTelemetryMotionId, payload);
    }

    bool FocMotorCategoryServer::BroadcastTelemetryCurrents(uint8_t sequence, foc::Ampere id, foc::Ampere iq, foc::Radians position,
        const infra::Function<void(bool sent)>& onDone)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(sequence);
        payload.WriteFixed16(id.Value(), focStreamCurrentScale);
        payload.WriteFixed16(iq.Value(), focStreamCurrentScale);
        payload.WriteFixed16(position.Value(), focStreamPositionScale);
        return payload.Valid() && Transport().SendFrame(services::CanPriority::telemetry, Id(), focTelemetryCurrentsId, payload.Message(), onDone);
    }

    bool FocMotorCategoryServer::SetpointMessages::HandleTorqueCurrent(const hal::Can::Message& data)
    {
        return HandleSetpoint(data, FocSetpointKind::torqueCurrent);
    }

    bool FocMotorCategoryServer::SetpointMessages::HandleTorqueNewtonMeter(const hal::Can::Message& data)
    {
        return HandleSetpoint(data, FocSetpointKind::torqueNewtonMeter);
    }

    bool FocMotorCategoryServer::SetpointMessages::HandleSpeed(const hal::Can::Message& data)
    {
        return HandleSetpoint(data, FocSetpointKind::speed);
    }

    bool FocMotorCategoryServer::SetpointMessages::HandlePosition(const hal::Can::Message& data)
    {
        return HandleSetpoint(data, FocSetpointKind::position);
    }

    bool FocMotorCategoryServer::SetpointMessages::HandleSetpoint(const hal::Can::Message& data, FocSetpointKind kind)
    {
        const auto commandId = SetpointCommandId(kind);
        if (!PayloadExact(data, commandId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const FocSetpoint setpoint{ kind, reader.ReadFloat32() };

        if (!reader.Valid() || !IsFiniteWireValue(setpoint.value))
            return false;

        server.NotifyObservers([this, setpoint, commandId](auto& observer)
            {
                observer.OnSetSetpoint(setpoint, [this, commandId]()
                    {
                        server.SendCommandAck(commandId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::MaintenanceMessages::HandleClearDiagnostics(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focClearDiagnosticsId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnClearDiagnostics([this]()
                    {
                        server.SendCommandAck(focClearDiagnosticsId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::MaintenanceMessages::HandleQueryFaultHistory(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focQueryFaultHistoryId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto age = reader.ReadUInt8();

        if (!reader.Valid())
            return false;

        server.NotifyObservers([this, age](auto& observer)
            {
                observer.OnQueryFaultHistory(age, [this](const FocFaultRecord& record)
                    {
                        server.SendFaultRecord(record);
                        server.SendCommandAck(focQueryFaultHistoryId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::MaintenanceMessages::HandleClearFaultHistory(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focClearFaultHistoryId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnClearFaultHistory([this](bool stored)
                    {
                        AcknowledgeStored(focClearFaultHistoryId, stored);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::MaintenanceMessages::HandleRecalibrate(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focRecalibrateId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnRecalibrate([this]()
                    {
                        server.SendCommandAck(focRecalibrateId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::MaintenanceMessages::HandleResetModel(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focResetModelId))
            return false;

        server.NotifyObservers([this](auto& observer)
            {
                observer.OnResetModel([this](bool stored)
                    {
                        AcknowledgeStored(focResetModelId, stored);
                    });
            });

        return true;
    }

    void FocMotorCategoryServer::MaintenanceMessages::AcknowledgeStored(uint8_t commandId, bool stored)
    {
        if (stored)
            server.SendCommandAck(commandId, services::CanAckStatus::success);
        else
            server.SendCategoryError(commandId, FocMotorCategoryError::persistenceFailed);
    }
}
