#include "source/services/can_protocol/client/CanProtocolClientImpl.hpp"

namespace services
{
    CanProtocolClientImpl::CanProtocolClientImpl(hal::Can& can, const Config& config)
        : transport(can, config.nodeId)
    {
        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    void CanProtocolClientImpl::SetNodeId(uint16_t nodeId)
    {
        transport.SetNodeId(nodeId);
    }

    uint16_t CanProtocolClientImpl::NodeId() const
    {
        return transport.NodeId();
    }

    uint8_t CanProtocolClientImpl::NextSequence()
    {
        return ++sequenceCounter;
    }

    void CanProtocolClientImpl::SendStartMotor(const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        transport.SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, payload, onDone);
    }

    void CanProtocolClientImpl::SendStopMotor(const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        transport.SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::stopMotor, payload, onDone);
    }

    void CanProtocolClientImpl::SendEmergencyStop(const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(0);
        transport.SendFrame(CanPriority::emergency, CanCategory::motorControl, CanMessageType::emergencyStop, payload, onDone);
    }

    void CanProtocolClientImpl::SendSetControlMode(CanControlMode mode, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        payload.push_back(static_cast<uint8_t>(mode));
        transport.SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setControlMode, payload, onDone);
    }

    void CanProtocolClientImpl::SendSetTorqueSetpoint(float idCurrent, float iqCurrent, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        CanFrameCodec::WriteInt16(payload, 1, CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale));
        CanFrameCodec::WriteInt16(payload, 3, CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale));
        transport.SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setTorqueSetpoint, payload, onDone);
    }

    void CanProtocolClientImpl::SendSetSpeedSetpoint(float speedRadPerSec, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        CanFrameCodec::WriteInt32(payload, 1, CanFrameCodec::FloatToFixed32(speedRadPerSec, canSpeedScale));
        transport.SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setSpeedSetpoint, payload, onDone);
    }

    void CanProtocolClientImpl::SendSetPositionSetpoint(float positionRad, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        CanFrameCodec::WriteInt32(payload, 1, CanFrameCodec::FloatToFixed32(positionRad, canPositionScale));
        transport.SendFrame(CanPriority::command, CanCategory::motorControl, CanMessageType::setPositionSetpoint, payload, onDone);
    }

    void CanProtocolClientImpl::SendPidGains(CanMessageType type, float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        CanFrameCodec::WriteInt16(payload, 1, CanFrameCodec::FloatToFixed16(kp, canPidGainScale));
        CanFrameCodec::WriteInt16(payload, 3, CanFrameCodec::FloatToFixed16(ki, canPidGainScale));
        CanFrameCodec::WriteInt16(payload, 5, CanFrameCodec::FloatToFixed16(kd, canPidGainScale));
        transport.SendFrame(CanPriority::command, CanCategory::pidTuning, type, payload, onDone);
    }

    void CanProtocolClientImpl::SendSetCurrentIdPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        SendPidGains(CanMessageType::setCurrentIdPid, kp, ki, kd, onDone);
    }

    void CanProtocolClientImpl::SendSetCurrentIqPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        SendPidGains(CanMessageType::setCurrentIqPid, kp, ki, kd, onDone);
    }

    void CanProtocolClientImpl::SendSetSpeedPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        SendPidGains(CanMessageType::setSpeedPid, kp, ki, kd, onDone);
    }

    void CanProtocolClientImpl::SendSetPositionPid(float kp, float ki, float kd, const infra::Function<void()>& onDone)
    {
        SendPidGains(CanMessageType::setPositionPid, kp, ki, kd, onDone);
    }

    void CanProtocolClientImpl::SendSetSupplyVoltage(float volts, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        CanFrameCodec::WriteInt16(payload, 1, CanFrameCodec::FloatToFixed16(volts, canVoltageScale));
        transport.SendFrame(CanPriority::command, CanCategory::systemParameters, CanMessageType::setSupplyVoltage, payload, onDone);
    }

    void CanProtocolClientImpl::SendSetMaxCurrent(float amps, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(NextSequence());
        CanFrameCodec::WriteInt16(payload, 1, CanFrameCodec::FloatToFixed16(amps, canCurrentScale));
        transport.SendFrame(CanPriority::command, CanCategory::systemParameters, CanMessageType::setMaxCurrent, payload, onDone);
    }

    void CanProtocolClientImpl::RequestData(DataRequestFlags flags, const infra::Function<void()>& onDone)
    {
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(flags));
        transport.SendFrame(CanPriority::command, CanCategory::system, CanMessageType::requestData, payload, onDone);
    }

    void CanProtocolClientImpl::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
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

    void CanProtocolClientImpl::HandleTelemetry(CanMessageType type, const hal::Can::Message& data) const
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

    void CanProtocolClientImpl::HandleMotorStatusTelemetry(const hal::Can::Message& data) const
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

    void CanProtocolClientImpl::HandleCurrentMeasurementTelemetry(const hal::Can::Message& data) const
    {
        if (data.size() < 4)
            return;

        auto id = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 0), canCurrentScale);
        auto iq = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 2), canCurrentScale);
        NotifyObservers([id, iq](auto& observer)
            {
                observer.OnCurrentMeasurementReceived(id, iq);
            });
    }

    void CanProtocolClientImpl::HandleSpeedPositionTelemetry(const hal::Can::Message& data) const
    {
        if (data.size() < 4)
            return;

        auto speed = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 0), canSpeedScale);
        auto pos = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 2), canPositionScale);
        NotifyObservers([speed, pos](auto& observer)
            {
                observer.OnSpeedPositionReceived(speed, pos);
            });
    }

    void CanProtocolClientImpl::HandleBusVoltageTelemetry(const hal::Can::Message& data) const
    {
        if (data.size() < 2)
            return;

        auto voltage = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 0), canVoltageScale);
        NotifyObservers([voltage](auto& observer)
            {
                observer.OnBusVoltageReceived(voltage);
            });
    }

    void CanProtocolClientImpl::HandleFaultEventTelemetry(const hal::Can::Message& data) const
    {
        if (data.size() < 1)
            return;

        auto fault = static_cast<CanFaultCode>(data[0]);
        NotifyObservers([fault](auto& observer)
            {
                observer.OnFaultEventReceived(fault);
            });
    }

    void CanProtocolClientImpl::HandleSystemMessage(CanMessageType type, const hal::Can::Message& data) const
    {
        if (type == CanMessageType::commandAck)
            HandleCommandAck(data);
    }

    void CanProtocolClientImpl::HandleCommandAck(const hal::Can::Message& data) const
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
}
