#include "source/services/can_protocol/client/CanProtocolClient.hpp"

namespace services
{
    CanProtocolClient::CanProtocolClient(hal::Can& can, const Config& config)
        : can(can)
        , config(config)
    {
        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    void CanProtocolClient::SetNodeId(uint16_t nodeId)
    {
        config.nodeId = nodeId;
    }

    uint16_t CanProtocolClient::NodeId() const
    {
        return config.nodeId;
    }

    uint8_t CanProtocolClient::NextSequence()
    {
        return ++sequenceCounter;
    }

    void CanProtocolClient::SendStartMotor(const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, payload, onDone);
    }

    void CanProtocolClient::SendStopMotor(const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::stopMotor, payload, onDone);
    }

    void CanProtocolClient::SendEmergencyStop(const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(0);
        SendFrame(CanPriority::emergency, CanCategory::motorControl, CanMessageType::emergencyStop, payload, onDone);
    }

    void CanProtocolClient::SendSetControlMode(CanControlMode mode, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(mode));
        SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setControlMode, payload, onDone);
    }

    void CanProtocolClient::SendSetTorqueSetpoint(float idCurrent, float iqCurrent, const infra::Function<void()>& onDone)
    {
        auto idFixed = CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale);
        auto iqFixed = CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(idFixed >> 8));
        payload.push_back(static_cast<uint8_t>(idFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(iqFixed >> 8));
        payload.push_back(static_cast<uint8_t>(iqFixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setTorqueSetpoint, payload, onDone);
    }

    void CanProtocolClient::SendSetSpeedSetpoint(float speedRadPerSec, const infra::Function<void()>& onDone)
    {
        auto fixed = CanFrameCodec::FloatToFixed32(speedRadPerSec, canSpeedScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setSpeedSetpoint, payload, onDone);
    }

    void CanProtocolClient::SendSetPositionSetpoint(float positionRad, const infra::Function<void()>& onDone)
    {
        auto fixed = CanFrameCodec::FloatToFixed32(positionRad, canPositionScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>((fixed >> 24) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((fixed >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setPositionSetpoint, payload, onDone);
    }

    void CanProtocolClient::SendSetCurrentIdPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        auto kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        auto kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        auto kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::pidTuning, CanMessageType::setCurrentIdPid, payload, onDone);
    }

    void CanProtocolClient::SendSetCurrentIqPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        auto kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        auto kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        auto kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::pidTuning, CanMessageType::setCurrentIqPid, payload, onDone);
    }

    void CanProtocolClient::SendSetSpeedPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        auto kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        auto kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        auto kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::pidTuning, CanMessageType::setSpeedPid, payload, onDone);
    }

    void CanProtocolClient::SendSetPositionPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        auto kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        auto kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        auto kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(kpFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kpFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kiFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kiFixed & 0xFF));
        payload.push_back(static_cast<uint8_t>(kdFixed >> 8));
        payload.push_back(static_cast<uint8_t>(kdFixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::pidTuning, CanMessageType::setPositionPid, payload, onDone);
    }

    void CanProtocolClient::SendSetSupplyVoltage(float volts, const infra::Function<void()>& onDone)
    {
        auto fixed = CanFrameCodec::FloatToFixed16(volts, canVoltageScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(fixed >> 8));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::systemParameters, CanMessageType::setSupplyVoltage, payload, onDone);
    }

    void CanProtocolClient::SendSetMaxCurrent(float amps, const infra::Function<void()>& onDone)
    {
        auto fixed = CanFrameCodec::FloatToFixed16(amps, canCurrentScale);

        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(fixed >> 8));
        payload.push_back(static_cast<uint8_t>(fixed & 0xFF));
        SendFrame(CanPriority::command, CanCategory::systemParameters, CanMessageType::setMaxCurrent, payload, onDone);
    }

    void CanProtocolClient::RequestData(DataRequestFlags flags, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(flags));
        SendFrame(CanPriority::command, CanCategory::system, CanMessageType::requestData, payload, onDone);
    }

    void CanProtocolClient::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();
        auto category = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        if (category == CanCategory::telemetry)
            HandleTelemetry(messageType, data);
        else if (category == CanCategory::system)
            HandleSystemMessage(messageType, data);
    }

    void CanProtocolClient::HandleTelemetry(CanMessageType type, const hal::Can::Message& data)
    {
        if (type == CanMessageType::motorStatus)
            HandleMotorStatusTelemetry(data);
        else if (type == CanMessageType::currentMeasurement)
            HandleCurrentMeasurementTelemetry(data);
        else if (type == CanMessageType::speedPosition)
            HandleSpeedPositionTelemetry(data);
        else if (type == CanMessageType::busVoltage)
            HandleBusVoltageTelemetry(data);
        else if (type == CanMessageType::faultEvent)
            HandleFaultEventTelemetry(data);
    }

    void CanProtocolClient::HandleMotorStatusTelemetry(const hal::Can::Message& data)
    {
        if (data.size() < 3)
            return;

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

    void CanProtocolClient::HandleCurrentMeasurementTelemetry(const hal::Can::Message& data)
    {
        if (data.size() < 4)
            return;

        auto idRaw = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
        auto iqRaw = static_cast<int16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
        auto id = CanFrameCodec::Fixed16ToFloat(idRaw, canCurrentScale);
        auto iq = CanFrameCodec::Fixed16ToFloat(iqRaw, canCurrentScale);
        NotifyObservers([id, iq](auto& observer)
            {
                observer.OnCurrentMeasurementReceived(id, iq);
            });
    }

    void CanProtocolClient::HandleSpeedPositionTelemetry(const hal::Can::Message& data)
    {
        if (data.size() < 4)
            return;

        auto speedRaw = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
        auto posRaw = static_cast<int16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
        auto speed = CanFrameCodec::Fixed16ToFloat(speedRaw, canSpeedScale);
        auto pos = CanFrameCodec::Fixed16ToFloat(posRaw, canPositionScale);
        NotifyObservers([speed, pos](auto& observer)
            {
                observer.OnSpeedPositionReceived(speed, pos);
            });
    }

    void CanProtocolClient::HandleBusVoltageTelemetry(const hal::Can::Message& data)
    {
        if (data.size() < 2)
            return;

        auto raw = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
        auto voltage = CanFrameCodec::Fixed16ToFloat(raw, canVoltageScale);
        NotifyObservers([voltage](auto& observer)
            {
                observer.OnBusVoltageReceived(voltage);
            });
    }

    void CanProtocolClient::HandleFaultEventTelemetry(const hal::Can::Message& data)
    {
        if (data.size() < 1)
            return;

        auto fault = static_cast<CanFaultCode>(data[0]);
        NotifyObservers([fault](auto& observer)
            {
                observer.OnFaultEventReceived(fault);
            });
    }

    void CanProtocolClient::HandleSystemMessage(CanMessageType type, const hal::Can::Message& data)
    {
        if (type == CanMessageType::commandAck)
            HandleCommandAck(data);
        // heartbeat silently consumed — no observer notification
    }

    void CanProtocolClient::HandleCommandAck(const hal::Can::Message& data)
    {
        if (data.size() < 3)
            return;

        auto category = static_cast<CanCategory>(data[0]);
        auto command = static_cast<CanMessageType>(data[1]);
        auto status = static_cast<CanAckStatus>(data[2]);
        NotifyObservers([category, command, status](auto& observer)
            {
                observer.OnCommandAckReceived(category, command, status);
            });
    }

    void CanProtocolClient::SendFrame(CanPriority priority, CanCategory category, CanMessageType type,
        const hal::Can::Message& data, const infra::Function<void()>& onDone)
    {
        auto rawId = MakeCanId(priority, category, type, config.nodeId);
        auto canId = hal::Can::Id::Create29BitId(rawId);

        if (!sendInProgress)
        {
            sendInProgress = true;
            currentOnDone = onDone;
            can.SendData(canId, data, [this](bool)
                {
                    auto done = currentOnDone;
                    SendNextQueued();
                    done();
                });
        }
        else if (!sendQueue.full())
        {
            sendQueue.push_back(PendingFrame{ canId, data, onDone });
        }
    }

    void CanProtocolClient::SendNextQueued()
    {
        if (sendQueue.empty())
        {
            sendInProgress = false;
            return;
        }

        auto frame = sendQueue.front();
        sendQueue.pop_front();
        currentOnDone = frame.onDone;

        can.SendData(frame.id, frame.data, [this](bool)
            {
                auto done = currentOnDone;
                SendNextQueued();
                done();
            });
    }
}
