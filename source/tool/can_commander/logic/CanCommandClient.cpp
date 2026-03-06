#include "source/tool/can_commander/logic/CanCommandClient.hpp"

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(CanBusAdapter& adapter, QObject* parent)
        : QObject(parent)
        , adapter(adapter)
    {
        timeoutTimer.setSingleShot(true);
        connect(&timeoutTimer, &QTimer::timeout, this, &CanCommandClient::OnTimeout);
        connect(&adapter, &CanBusAdapter::FrameReceived, this, &CanCommandClient::OnFrameReceived);
    }

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
            emit BusyChanged(busy);
        }
    }

    void CanCommandClient::SendCommand(CanPriority priority, CanCategory category,
        CanMessageType type, const QByteArray& payload)
    {
        uint32_t rawId = MakeCanId(priority, category, type, nodeId);
        adapter.Send(rawId, payload);
        emit FrameLog("TX", rawId, payload);

        SetBusy(true);
        timeoutTimer.start(commandTimeoutMs);
    }

    void CanCommandClient::SendStartMotor()
    {
        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, payload);
    }

    void CanCommandClient::SendStopMotor()
    {
        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::stopMotor, payload);
    }

    void CanCommandClient::SendEmergencyStop()
    {
        QByteArray payload;
        payload.append(static_cast<char>(0));
        SendCommand(CanPriority::emergency, CanCategory::motorControl, CanMessageType::emergencyStop, payload);
    }

    void CanCommandClient::SendSetControlMode(CanControlMode mode)
    {
        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(mode)));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setControlMode, payload);
    }

    void CanCommandClient::SendSetTorqueSetpoint(float idCurrent, float iqCurrent)
    {
        int16_t idFixed = CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale);
        int16_t iqFixed = CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(idFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(idFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(iqFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(iqFixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setTorqueSetpoint, payload);
    }

    void CanCommandClient::SendSetSpeedSetpoint(float speedRadPerSec)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(speedRadPerSec, canSpeedScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 24) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 16) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 8) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setSpeedSetpoint, payload);
    }

    void CanCommandClient::SendSetPositionSetpoint(float positionRad)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(positionRad, canPositionScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 24) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 16) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 8) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::motorControl, CanMessageType::setPositionSetpoint, payload);
    }

    void CanCommandClient::SendSetCurrentIdPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setCurrentIdPid, payload);
    }

    void CanCommandClient::SendSetCurrentIqPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setCurrentIqPid, payload);
    }

    void CanCommandClient::SendSetSpeedPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setSpeedPid, payload);
    }

    void CanCommandClient::SendSetPositionPid(float kp, float ki, float kd)
    {
        int16_t kpFixed = CanFrameCodec::FloatToFixed16(kp, canPidGainScale);
        int16_t kiFixed = CanFrameCodec::FloatToFixed16(ki, canPidGainScale);
        int16_t kdFixed = CanFrameCodec::FloatToFixed16(kd, canPidGainScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kpFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kiFixed & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(kdFixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::pidTuning, CanMessageType::setPositionPid, payload);
    }

    void CanCommandClient::SendSetPolePairs(uint8_t polePairs)
    {
        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(polePairs));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setPolePairs, payload);
    }

    void CanCommandClient::SendSetResistance(float ohms)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(ohms, canResistanceScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 24) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 16) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 8) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setResistance, payload);
    }

    void CanCommandClient::SendSetInductance(float henries)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(henries, canInductanceScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 24) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 16) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 8) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setInductance, payload);
    }

    void CanCommandClient::SendSetFluxLinkage(float webers)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(webers, canFluxScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 24) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 16) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>((fixed >> 8) & 0xFF)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::motorParameters, CanMessageType::setFluxLinkage, payload);
    }

    void CanCommandClient::SendSetSupplyVoltage(float volts)
    {
        int16_t fixed = CanFrameCodec::FloatToFixed16(volts, canVoltageScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::systemParameters, CanMessageType::setSupplyVoltage, payload);
    }

    void CanCommandClient::SendSetMaxCurrent(float amps)
    {
        int16_t fixed = CanFrameCodec::FloatToFixed16(amps, canCurrentScale);

        QByteArray payload;
        payload.append(static_cast<char>(NextSequence()));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed >> 8)));
        payload.append(static_cast<char>(static_cast<uint8_t>(fixed & 0xFF)));
        SendCommand(CanPriority::command, CanCategory::systemParameters, CanMessageType::setMaxCurrent, payload);
    }

    void CanCommandClient::SendHeartbeat()
    {
        QByteArray payload;
        payload.append(static_cast<char>(canProtocolVersion));

        uint32_t rawId = MakeCanId(CanPriority::heartbeat, CanCategory::system, CanMessageType::heartbeat, nodeId);
        adapter.Send(rawId, payload);
        emit FrameLog("TX", rawId, payload);
    }

    void CanCommandClient::SendRequestStatus()
    {
        QByteArray payload;

        uint32_t rawId = MakeCanId(CanPriority::command, CanCategory::system, CanMessageType::requestStatus, nodeId);
        adapter.Send(rawId, payload);
        emit FrameLog("TX", rawId, payload);
    }

    void CanCommandClient::OnFrameReceived(uint32_t id, QByteArray data)
    {
        emit FrameLog("RX", id, data);

        auto category = ExtractCanCategory(id);
        auto messageType = ExtractCanMessageType(id);

        if (category == CanCategory::telemetry)
            HandleTelemetry(messageType, data);
        else if (category == CanCategory::system)
            HandleSystemMessage(messageType, data);
    }

    void CanCommandClient::HandleTelemetry(CanMessageType type, const QByteArray& data)
    {
        if (type == CanMessageType::motorStatus && data.size() >= 3)
        {
            auto mode = static_cast<CanControlMode>(static_cast<uint8_t>(data[1]));
            emit MotorStatusReceived(
                static_cast<CanMotorState>(static_cast<uint8_t>(data[0])),
                mode,
                static_cast<CanFaultCode>(static_cast<uint8_t>(data[2])));
            emit ControlModeReceived(mode);
        }
        else if (type == CanMessageType::currentMeasurement && data.size() >= 4)
        {
            int16_t idRaw = static_cast<int16_t>((static_cast<uint16_t>(static_cast<uint8_t>(data[0])) << 8) | static_cast<uint8_t>(data[1]));
            int16_t iqRaw = static_cast<int16_t>((static_cast<uint16_t>(static_cast<uint8_t>(data[2])) << 8) | static_cast<uint8_t>(data[3]));
            emit CurrentMeasurementReceived(
                CanFrameCodec::Fixed16ToFloat(idRaw, canCurrentScale),
                CanFrameCodec::Fixed16ToFloat(iqRaw, canCurrentScale));
        }
        else if (type == CanMessageType::speedPosition && data.size() >= 4)
        {
            int16_t speedRaw = static_cast<int16_t>((static_cast<uint16_t>(static_cast<uint8_t>(data[0])) << 8) | static_cast<uint8_t>(data[1]));
            int16_t posRaw = static_cast<int16_t>((static_cast<uint16_t>(static_cast<uint8_t>(data[2])) << 8) | static_cast<uint8_t>(data[3]));
            emit SpeedPositionReceived(
                CanFrameCodec::Fixed16ToFloat(speedRaw, canSpeedScale),
                CanFrameCodec::Fixed16ToFloat(posRaw, canPositionScale));
        }
        else if (type == CanMessageType::busVoltage && data.size() >= 2)
        {
            int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(static_cast<uint8_t>(data[0])) << 8) | static_cast<uint8_t>(data[1]));
            emit BusVoltageReceived(CanFrameCodec::Fixed16ToFloat(raw, canVoltageScale));
        }
        else if (type == CanMessageType::faultEvent && data.size() >= 1)
        {
            emit FaultEventReceived(static_cast<CanFaultCode>(static_cast<uint8_t>(data[0])));
        }
    }

    void CanCommandClient::HandleSystemMessage(CanMessageType type, const QByteArray& data)
    {
        if (type == CanMessageType::commandAck && data.size() >= 3)
        {
            timeoutTimer.stop();
            SetBusy(false);

            auto category = static_cast<CanCategory>(static_cast<uint8_t>(data[0]));
            auto command = static_cast<CanMessageType>(static_cast<uint8_t>(data[1]));
            auto status = static_cast<CanAckStatus>(static_cast<uint8_t>(data[2]));
            emit CommandAckReceived(category, command, status);
        }
        else if (type == CanMessageType::heartbeat && data.size() >= 1)
        {
            emit HeartbeatReceived(static_cast<uint8_t>(data[0]));
        }
    }

    void CanCommandClient::OnTimeout()
    {
        SetBusy(false);
        emit CommandTimeout();
    }
}
