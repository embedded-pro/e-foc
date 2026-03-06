#include "source/tool/can_commander/logic/CanCommandClient.hpp"

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(CanBusAdapter& adapter)
        : CanBusAdapterObserver(adapter)
        , adapter(adapter)
    {}

    void CanCommandClient::SetNodeId(uint16_t id)
    {
        nodeId = id;
    }

    uint16_t CanCommandClient::NodeId() const
    {
        return nodeId;
    }

    bool CanCommandClient::IsBusy() const
    {
        return busy;
    }

    uint8_t CanCommandClient::NextSequence()
    {
        return ++sequenceCounter;
    }

    void CanCommandClient::SetBusy(bool newBusy)
    {
        if (busy != newBusy)
        {
            busy = newBusy;
            NotifyObservers([newBusy](auto& observer)
                {
                    observer.OnBusyChanged(newBusy);
                });
        }
    }

    void CanCommandClient::SendCommand(CanPriority priority, CanCategory category,
        CanMessageType type, const CanFrame& payload)
    {
        uint32_t rawId = MakeCanId(priority, category, type, nodeId);
        adapter.Send(rawId, payload);
        NotifyObservers([rawId, &payload](auto& observer)
            {
                observer.OnFrameLog(true, rawId, payload);
            });

        SetBusy(true);
    }

    void CanCommandClient::SendStartMotor()
    {
        CanFrame payload;
        payload.push_back(NextSequence());
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, payload);
    }

    void CanCommandClient::SendStopMotor()
    {
        CanFrame payload;
        payload.push_back(NextSequence());
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::stopMotor, payload);
    }

    void CanCommandClient::SendEmergencyStop()
    {
        CanFrame payload;
        payload.push_back(0);
        SendCommand(CanPriority::emergency, CanCategory::motorControl, CanMessageType::emergencyStop, payload);
    }

    void CanCommandClient::SendSetControlMode(CanControlMode mode)
    {
        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(mode));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setControlMode, payload);
    }

    void CanCommandClient::SendSetTorqueSetpoint(float idCurrent, float iqCurrent)
    {
        int16_t idFixed = CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale);
        int16_t iqFixed = CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(idFixed >> 8));
        payload.push_back(static_cast<uint8_t>(idFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(iqFixed >> 8));
        payload.push_back(static_cast<uint8_t>(iqFixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setTorqueSetpoint, payload);
    }

    void CanCommandClient::SendSetSpeedSetpoint(float speedRadPerSec)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(speedRadPerSec, canSpeedScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setSpeedSetpoint, payload);
    }

    void CanCommandClient::SendSetPositionSetpoint(float positionRad)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(positionRad, canPositionScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setPositionSetpoint, payload);
    }

    void CanCommandClient::SendSetCurrentIdPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setCurrentIdPid, payload);
    }

    void CanCommandClient::SendSetCurrentIqPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setCurrentIqPid, payload);
    }

    void CanCommandClient::SendSetSpeedPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setSpeedPid, payload);
    }

    void CanCommandClient::SendSetPositionPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setPositionPid, payload);
    }

    void CanCommandClient::SendSetPolePairs(uint8_t polePairs)
    {
        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(polePairs);
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setPolePairs, payload);
    }

    void CanCommandClient::SendSetResistance(float ohms)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(ohms, canResistanceScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setResistance, payload);
    }

    void CanCommandClient::SendSetInductance(float henries)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(henries, canInductanceScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setInductance, payload);
    }

    void CanCommandClient::SendSetFluxLinkage(float webers)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(webers, canFluxScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setFluxLinkage, payload);
    }

    void CanCommandClient::SendSetSupplyVoltage(float volts)
    {
        int16_t fixed = CanFrameCodec::FloatToFixed16(volts, canVoltageScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(fixed >> 8));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::systemParameters, CanMessageType::setSupplyVoltage, payload);
    }

    void CanCommandClient::SendSetMaxCurrent(float amps)
    {
        int16_t fixed = CanFrameCodec::FloatToFixed16(amps, canCurrentScale);

        CanFrame payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(fixed >> 8));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendCommand(CanPriority::command, CanCategory::systemParameters, CanMessageType::setMaxCurrent, payload);
    }

    void CanCommandClient::SendHeartbeat()
    {
        CanFrame payload;
        payload.push_back(canProtocolVersion);

        uint32_t rawId = MakeCanId(CanPriority::heartbeat, CanCategory::system, CanMessageType::heartbeat, nodeId);
        adapter.Send(rawId, payload);
        NotifyObservers([rawId, &payload](auto& observer)
            {
                observer.OnFrameLog(true, rawId, payload);
            });
    }

    void CanCommandClient::SendRequestStatus()
    {
        CanFrame payload;

        uint32_t rawId = MakeCanId(CanPriority::command, CanCategory::system, CanMessageType::requestStatus, nodeId);
        adapter.Send(rawId, payload);
        NotifyObservers([rawId, &payload](auto& observer)
            {
                observer.OnFrameLog(true, rawId, payload);
            });
    }

    void CanCommandClient::HandleTimeout()
    {
        SetBusy(false);
        NotifyObservers([](auto& observer)
            {
                observer.OnCommandTimeout();
            });
    }

    void CanCommandClient::OnFrameReceived(uint32_t id, const CanFrame& data)
    {
        NotifyObservers([id, &data](auto& observer)
            {
                observer.OnFrameLog(false, id, data);
            });

        auto category = ExtractCanCategory(id);
        auto messageType = ExtractCanMessageType(id);

        if (category == CanCategory::telemetry)
            HandleTelemetry(messageType, data);
        else if (category == CanCategory::system)
            HandleSystemMessage(messageType, data);
    }

    void CanCommandClient::OnError(infra::BoundedConstString message)
    {
        NotifyObservers([message](auto& observer)
            {
                observer.OnAdapterError(message);
            });
    }

    void CanCommandClient::OnConnectionChanged(bool connected)
    {
        NotifyObservers([connected](auto& observer)
            {
                observer.OnConnectionChanged(connected);
            });
    }

    void CanCommandClient::HandleTelemetry(CanMessageType type, const CanFrame& data)
    {
        if (type == CanMessageType::motorStatus && data.size() >= 3)
        {
            auto state = static_cast<CanMotorState>(data[0]);
            auto mode = static_cast<CanControlMode>(data[1]);
            auto fault = static_cast<CanFaultCode>(data[2]);
            NotifyObservers([state, mode, fault](auto& observer)
                {
                    observer.OnMotorStatusReceived(state, mode, fault);
                });
            NotifyObservers([mode](auto& observer)
                {
                    observer.OnControlModeReceived(mode);
                });
        }
        else if (type == CanMessageType::currentMeasurement && data.size() >= 4)
        {
            int16_t idRaw = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
            int16_t iqRaw = static_cast<int16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
            float id = CanFrameCodec::Fixed16ToFloat(idRaw, canCurrentScale);
            float iq = CanFrameCodec::Fixed16ToFloat(iqRaw, canCurrentScale);
            NotifyObservers([id, iq](auto& observer)
                {
                    observer.OnCurrentMeasurementReceived(id, iq);
                });
        }
        else if (type == CanMessageType::speedPosition && data.size() >= 4)
        {
            int16_t speedRaw = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
            int16_t posRaw = static_cast<int16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
            float speed = CanFrameCodec::Fixed16ToFloat(speedRaw, canSpeedScale);
            float pos = CanFrameCodec::Fixed16ToFloat(posRaw, canPositionScale);
            NotifyObservers([speed, pos](auto& observer)
                {
                    observer.OnSpeedPositionReceived(speed, pos);
                });
        }
        else if (type == CanMessageType::busVoltage && data.size() >= 2)
        {
            int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
            float voltage = CanFrameCodec::Fixed16ToFloat(raw, canVoltageScale);
            NotifyObservers([voltage](auto& observer)
                {
                    observer.OnBusVoltageReceived(voltage);
                });
        }
        else if (type == CanMessageType::faultEvent && data.size() >= 1)
        {
            auto fault = static_cast<CanFaultCode>(data[0]);
            NotifyObservers([fault](auto& observer)
                {
                    observer.OnFaultEventReceived(fault);
                });
        }
    }

    void CanCommandClient::HandleSystemMessage(CanMessageType type, const CanFrame& data)
    {
        if (type == CanMessageType::commandAck && data.size() >= 3)
        {
            SetBusy(false);

            auto category = static_cast<CanCategory>(data[0]);
            auto command = static_cast<CanMessageType>(data[1]);
            auto status = static_cast<CanAckStatus>(data[2]);
            NotifyObservers([category, command, status](auto& observer)
                {
                    observer.OnCommandAckReceived(category, command, status);
                });
        }
        else if (type == CanMessageType::heartbeat && data.size() >= 1)
        {
            uint8_t version = data[0];
            NotifyObservers([version](auto& observer)
                {
                    observer.OnHeartbeatReceived(version);
                });
        }
    }
}
