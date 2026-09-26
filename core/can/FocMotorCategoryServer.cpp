#include "core/can/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "core/can/FocMotorWireContract.hpp"

namespace can
{
    FocMotorCategoryServer::FocMotorCategoryServer(services::CanFrameTransport& transport)
        : CanCategoryServer(transport)
    {
        AddMessageTypes(start, stop, clearFault, emergencyStop, selectControlMode,
            setTorqueSetpoint, setSpeedSetpoint, setPositionSetpoint,
            setPidCurrent, setPidSpeed, setPidPosition,
            align, identifyElectrical, identifyMechanical, requestTelemetry,
            setEncoderResolution, queryMotorType, configureTelemetryRate, queryContractVersion);
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
        payload.WriteFixed16(0.0f, focSpeedScale);
        payload.WriteFixed16(0.0f, focPositionScale);

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

    bool FocMotorCategoryServer::HandleSetTorqueSetpoint(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetTorqueSetpointId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto value = foc::Ampere{ reader.ReadFixed16(focCurrentScale) };

        if (!reader.Valid())
            return false;

        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetTorqueSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetTorqueSetpointId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleSetSpeedSetpoint(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetSpeedSetpointId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto value = foc::RadiansPerSecond{ reader.ReadFixed16(focSpeedScale) };

        if (!reader.Valid())
            return false;

        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetSpeedSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetSpeedSetpointId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleSetPositionSetpoint(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetPositionSetpointId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto value = foc::Radians{ reader.ReadFixed16(focPositionScale) };

        if (!reader.Valid())
            return false;

        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetPositionSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetPositionSetpointId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleSetPidCurrent(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetPidCurrentId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto bandwidth = reader.ReadFixed16(focPidScale);

        if (!reader.Valid())
            return false;

        NotifyObservers([this, bandwidth](auto& observer)
            {
                observer.OnSetPidCurrent(bandwidth, [this]()
                    {
                        SendCommandAck(focSetPidCurrentId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleSetPidSpeed(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetPidSpeedId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto bandwidth = reader.ReadFixed16(focPidScale);

        if (!reader.Valid())
            return false;

        NotifyObservers([this, bandwidth](auto& observer)
            {
                observer.OnSetPidSpeed(bandwidth, [this]()
                    {
                        SendCommandAck(focSetPidSpeedId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleSetPidPosition(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetPidPositionId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto bandwidth = reader.ReadFixed16(focPidScale);

        if (!reader.Valid())
            return false;

        NotifyObservers([this, bandwidth](auto& observer)
            {
                observer.OnSetPidPosition(bandwidth, [this]()
                    {
                        SendCommandAck(focSetPidPositionId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleAlign(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focAlignId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnAlign([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focAlignId, status);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleIdentifyElectrical(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focIdentifyElectricalId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyElectrical([this]()
                    {
                        SendCommandAck(focIdentifyElectricalId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleIdentifyMechanical(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focIdentifyMechanicalId))
            return false;

        NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyMechanical([this]()
                    {
                        SendCommandAck(focIdentifyMechanicalId, services::CanAckStatus::success);
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

    bool FocMotorCategoryServer::HandleSetEncoderResolution(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focSetEncoderResolutionId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto resolution = reader.ReadUInt32();

        if (!reader.Valid())
            return false;

        NotifyObservers([this, resolution](auto& observer)
            {
                observer.OnSetEncoderResolution(resolution, [this]()
                    {
                        SendCommandAck(focSetEncoderResolutionId, services::CanAckStatus::success);
                    });
            });

        return true;
    }

    bool FocMotorCategoryServer::HandleQueryMotorType(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focQueryMotorTypeId))
            return false;

        SendCommandAck(focQueryMotorTypeId, services::CanAckStatus::notImplemented);

        return true;
    }

    bool FocMotorCategoryServer::HandleConfigureTelemetryRate(const hal::Can::Message& data)
    {
        if (!PayloadExact(data, focConfigureTelemetryRateId))
            return false;

        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto rateHz = reader.ReadUInt32();

        if (!reader.Valid())
            return false;

        NotifyObservers([this, rateHz](auto& observer)
            {
                observer.OnConfigureTelemetryRate(rateHz, [this]()
                    {
                        SendCommandAck(focConfigureTelemetryRateId, services::CanAckStatus::success);
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

    void FocMotorCategoryServer::BroadcastTelemetryStatus(FocMotorState state, FocFaultCode fault,
        foc::RadiansPerSecond speed, foc::Radians position)
    {
        services::CanPayloadWriter payload;
        payload.WriteUInt8(static_cast<uint8_t>(state));
        payload.WriteUInt8(static_cast<uint8_t>(fault));
        payload.WriteFixed16(speed.Value(), focSpeedScale);
        payload.WriteFixed16(position.Value(), focPositionScale);
        SendTelemetry(focTelemetryStatusResponseId, payload);
    }

    void FocMotorCategoryServer::BroadcastElectricalParams(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs)
    {
        services::CanPayloadWriter payload;
        payload.WriteFixed16(resistance.Value(), focResistanceScale);
        payload.WriteFixed16(inductance.Value(), focInductanceScale);
        payload.WriteUInt8(static_cast<uint8_t>(polePairs));
        SendResponse(focElectricalParamsResponseId, payload);
    }

    void FocMotorCategoryServer::BroadcastMechanicalParams(foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia)
    {
        services::CanPayloadWriter payload;
        payload.WriteFixed16(friction.Value(), focFrictionScale);
        payload.WriteFixed16(inertia.Value(), focInertiaScale);
        SendResponse(focMechanicalParamsResponseId, payload);
    }
}
