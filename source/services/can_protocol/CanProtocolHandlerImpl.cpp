#include "source/services/can_protocol/CanProtocolHandlerImpl.hpp"

namespace services
{
    CanProtocolHandlerImpl::CanProtocolHandlerImpl(hal::Can& can, const Config& config)
        : can(can)
        , config(config)
        , motorControlHandler(*this)
        , pidTuningHandler(*this)
        , motorParametersHandler(*this)
        , systemParametersHandler(*this)
        , systemHandler(*this)
        , handlers{ { &motorControlHandler, &pidTuningHandler, &motorParametersHandler, &systemParametersHandler, &systemHandler } }
    {}

    void CanProtocolHandlerImpl::StartListening()
    {
        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    void CanProtocolHandlerImpl::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!id.Is29BitId())
            return;

        uint32_t rawId = id.Get29BitId();
        uint16_t targetNodeId = ExtractCanNodeId(rawId);

        if (targetNodeId != config.nodeId && targetNodeId != canBroadcastNodeId)
            return;

        if (!CheckAndIncrementRate())
            return;

        auto category = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        CanCategoryHandler* handler = FindHandler(category);
        if (handler == nullptr)
            return;

        if (handler->RequiresSequenceValidation() && messageType != CanMessageType::emergencyStop)
        {
            if (!data.empty())
            {
                uint8_t sequenceNumber = data[0];
                if (!ValidateSequence(sequenceNumber))
                {
                    SendCommandAck(messageType, CanAckStatus::sequenceError);
                    return;
                }
            }
        }

        handler->Handle(messageType, data);
    }

    CanCategoryHandler* CanProtocolHandlerImpl::FindHandler(CanCategory category)
    {
        for (auto* handler : handlers)
        {
            if (handler->Category() == category)
                return handler;
        }

        return nullptr;
    }

    void CanProtocolHandlerImpl::SendMotorStatus(CanMotorState state, CanControlMode mode, CanFaultCode fault)
    {
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(state));
        msg.push_back(static_cast<uint8_t>(mode));
        msg.push_back(static_cast<uint8_t>(fault));

        SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::motorStatus, msg);
    }

    void CanProtocolHandlerImpl::SendCurrentMeasurement(float idCurrent, float iqCurrent)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale));
        CanFrameCodec::WriteInt16(msg, 2, CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale));

        SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::currentMeasurement, msg);
    }

    void CanProtocolHandlerImpl::SendSpeedPosition(float speed, float position)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, CanFrameCodec::FloatToFixed16(speed, canSpeedScale));
        CanFrameCodec::WriteInt16(msg, 2, CanFrameCodec::FloatToFixed16(position, canPositionScale));

        SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::speedPosition, msg);
    }

    void CanProtocolHandlerImpl::SendBusVoltage(float voltage)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, CanFrameCodec::FloatToFixed16(voltage, canVoltageScale));

        SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::busVoltage, msg);
    }

    void CanProtocolHandlerImpl::SendHeartbeat()
    {
        hal::Can::Message msg;
        msg.push_back(canProtocolVersion);

        SendFrame(CanPriority::heartbeat, CanCategory::system, CanMessageType::heartbeat, msg);
    }

    void CanProtocolHandlerImpl::SendFaultEvent(CanFaultCode fault)
    {
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(fault));

        SendFrame(CanPriority::emergency, CanCategory::telemetry, CanMessageType::faultEvent, msg);
    }

    void CanProtocolHandlerImpl::SendCommandAck(CanMessageType commandType, CanAckStatus status)
    {
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(commandType));
        msg.push_back(static_cast<uint8_t>(status));

        SendFrame(CanPriority::response, CanCategory::system, CanMessageType::commandAck, msg);
    }

    void CanProtocolHandlerImpl::ResetRateCounter()
    {
        messageCountThisPeriod = 0;
    }

    bool CanProtocolHandlerImpl::IsRateLimited() const
    {
        return messageCountThisPeriod >= config.maxMessagesPerSecond;
    }

    bool CanProtocolHandlerImpl::CheckAndIncrementRate()
    {
        if (messageCountThisPeriod >= config.maxMessagesPerSecond)
            return false;

        ++messageCountThisPeriod;
        return true;
    }

    bool CanProtocolHandlerImpl::ValidateSequence(uint8_t sequenceNumber)
    {
        if (!sequenceInitialized)
        {
            sequenceInitialized = true;
            lastSequenceNumber = sequenceNumber;
            return true;
        }

        uint8_t expected = static_cast<uint8_t>(lastSequenceNumber + 1);
        if (sequenceNumber != expected)
            return false;

        lastSequenceNumber = sequenceNumber;
        return true;
    }

    void CanProtocolHandlerImpl::SendFrame(CanPriority priority, CanCategory category, CanMessageType type, const hal::Can::Message& data)
    {
        uint32_t rawId = MakeCanId(priority, category, type, config.nodeId);
        auto canId = hal::Can::Id::Create29BitId(rawId);

        can.SendData(canId, data, [](bool) {});
    }
}
