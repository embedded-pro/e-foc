#include "core/can/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace can
{
    FocMotorCategoryServer::FocMotorCategoryServer(services::CanFrameTransport& transport)
        : CanCategoryServer(transport)
    {
        AddMessageTypes(start, stop, clearFault, emergencyStop, selectControlMode,
            setTorqueSetpoint, setSpeedSetpoint, setPositionSetpoint,
            setPidCurrent, setPidSpeed, setPidPosition,
            identifyElectrical, identifyMechanical, requestTelemetry,
            setEncoderResolution, queryMotorType, configureTelemetryRate);
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

    void FocMotorCategoryServer::HandleStart(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnStart([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focStartId, status);
                    });
            });
    }

    void FocMotorCategoryServer::HandleStop(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnStop([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focStopId, status);
                    });
            });
    }

    void FocMotorCategoryServer::HandleClearFault(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnClearFault([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focClearFaultId, status);
                    });
            });
    }

    void FocMotorCategoryServer::HandleEmergencyStop(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnEmergencyStop([this](services::CanAckStatus status)
                    {
                        SendCommandAck(focEmergencyStopId, status);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSelectControlMode(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        auto rawMode = reader.ReadUInt8();
        if (!reader.Valid())
        {
            SendCommandAck(focSelectControlModeId, services::CanAckStatus::invalidPayload);
            return;
        }
        if (rawMode > static_cast<uint8_t>(FocMotorMode::position))
        {
            SendCommandAck(focSelectControlModeId, services::CanAckStatus::invalidPayload);
            return;
        }
        const auto mode = static_cast<FocMotorMode>(rawMode);
        NotifyObservers([this, mode](auto& observer)
            {
                observer.OnSelectControlMode(mode, [this](FocMotorMode activatedMode)
                    {
                        SendSelectControlModeResponse(activatedMode);
                        SendCommandAck(focSelectControlModeId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetTorqueSetpoint(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto value = foc::Ampere{ reader.ReadFixed16(focCurrentScale) };
        if (!reader.Valid())
        {
            SendCommandAck(focSetTorqueSetpointId, services::CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetTorqueSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetTorqueSetpointId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetSpeedSetpoint(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto value = foc::RadiansPerSecond{ reader.ReadFixed16(focSpeedScale) };
        if (!reader.Valid())
        {
            SendCommandAck(focSetSpeedSetpointId, services::CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetSpeedSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetSpeedSetpointId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPositionSetpoint(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        const auto value = foc::Radians{ reader.ReadFixed16(focPositionScale) };
        if (!reader.Valid())
        {
            SendCommandAck(focSetPositionSetpointId, services::CanAckStatus::invalidPayload);
            return;
        }
        NotifyObservers([this, value](auto& observer)
            {
                observer.OnSetPositionSetpoint(value, [this]()
                    {
                        SendCommandAck(focSetPositionSetpointId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPidCurrent(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnSetPidCurrent([this]()
                    {
                        SendCommandAck(focSetPidCurrentId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPidSpeed(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnSetPidSpeed([this]()
                    {
                        SendCommandAck(focSetPidSpeedId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetPidPosition(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnSetPidPosition([this]()
                    {
                        SendCommandAck(focSetPidPositionId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleIdentifyElectrical(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyElectrical([this]()
                    {
                        SendCommandAck(focIdentifyElectricalId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleIdentifyMechanical(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnIdentifyMechanical([this]()
                    {
                        SendCommandAck(focIdentifyMechanicalId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleRequestTelemetry(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnRequestTelemetry([this]()
                    {
                        SendCommandAck(focRequestTelemetryId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleSetEncoderResolution(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnSetEncoderResolution([this]()
                    {
                        SendCommandAck(focSetEncoderResolutionId, services::CanAckStatus::success);
                    });
            });
    }

    void FocMotorCategoryServer::HandleQueryMotorType(const hal::Can::Message&)
    {
        SendCommandAck(focQueryMotorTypeId, services::CanAckStatus::notImplemented);
    }

    void FocMotorCategoryServer::HandleConfigureTelemetryRate(const hal::Can::Message&)
    {
        NotifyObservers([this](auto& observer)
            {
                observer.OnConfigureTelemetryRate([this]()
                    {
                        SendCommandAck(focConfigureTelemetryRateId, services::CanAckStatus::success);
                    });
            });
    }
}
