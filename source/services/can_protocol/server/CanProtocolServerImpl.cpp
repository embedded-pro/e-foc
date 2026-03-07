#include "source/services/can_protocol/server/CanProtocolServerImpl.hpp"

namespace services
{
    CanProtocolServerImpl::CanProtocolServerImpl(hal::Can& can, const Config& config, Handlers& handlers)
        : config(config)
        , transport(can, config.nodeId)
        , handlers(handlers)
    {
        can.ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                ProcessReceivedMessage(id, data);
            });
    }

    void CanProtocolServerImpl::ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data)
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

        if (handler->RequiresSequenceValidation() && !(category == CanCategory::motorControl && messageType == CanMessageType::emergencyStop))
        {
            if (data.empty())
            {
                SendCommandAck(category, messageType, CanAckStatus::invalidPayload);
                return;
            }

            uint8_t sequenceNumber = data[0];
            if (!ValidateSequence(sequenceNumber))
            {
                SendCommandAck(category, messageType, CanAckStatus::sequenceError);
                return;
            }
        }

        handler->Handle(messageType, data);
    }

    CanCategoryHandler* CanProtocolServerImpl::FindHandler(CanCategory category)
    {
        for (auto* handler : handlers)
        {
            if (handler->Category() == category)
                return handler;
        }

        return nullptr;
    }

    void CanProtocolServerImpl::HandleDataRequest(DataRequestFlags flags)
    {
        DataResponse response;
        NotifyObservers([flags, &response](auto& observer)
            {
                observer.OnDataRequested(flags, response);
            });

        if (HasFlag(flags, DataRequestFlags::motorStatus))
            SendMotorStatus(response.motorState, response.controlMode, response.faultCode);
        if (HasFlag(flags, DataRequestFlags::currentMeasurement))
            SendCurrentMeasurement(response.idCurrent, response.iqCurrent);
        if (HasFlag(flags, DataRequestFlags::speedPosition))
            SendSpeedPosition(response.speed, response.position);
        if (HasFlag(flags, DataRequestFlags::busVoltage))
            SendBusVoltage(response.busVoltage);
        if (HasFlag(flags, DataRequestFlags::faultEvent))
            SendFaultEvent(response.faultCode);
    }

    void CanProtocolServerImpl::SendMotorStatus(CanMotorState state, CanControlMode mode, CanFaultCode fault)
    {
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(state));
        msg.push_back(static_cast<uint8_t>(mode));
        msg.push_back(static_cast<uint8_t>(fault));

        transport.SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::motorStatus, msg, [] {});
    }

    void CanProtocolServerImpl::SendCurrentMeasurement(float idCurrent, float iqCurrent)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale));
        CanFrameCodec::WriteInt16(msg, 2, CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale));

        transport.SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::currentMeasurement, msg, [] {});
    }

    void CanProtocolServerImpl::SendSpeedPosition(float speed, float position)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, CanFrameCodec::FloatToFixed16(speed, canSpeedScale));
        CanFrameCodec::WriteInt16(msg, 2, CanFrameCodec::FloatToFixed16(position, canPositionScale));

        transport.SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::speedPosition, msg, [] {});
    }

    void CanProtocolServerImpl::SendBusVoltage(float voltage)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, CanFrameCodec::FloatToFixed16(voltage, canVoltageScale));

        transport.SendFrame(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::busVoltage, msg, [] {});
    }

    void CanProtocolServerImpl::SendFaultEvent(CanFaultCode fault)
    {
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(fault));

        transport.SendFrame(CanPriority::emergency, CanCategory::telemetry, CanMessageType::faultEvent, msg, [] {});
    }

    void CanProtocolServerImpl::SendCommandAck(CanCategory category, CanMessageType commandType, CanAckStatus status, const infra::Function<void()>& onDone)
    {
        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(category));
        msg.push_back(static_cast<uint8_t>(commandType));
        msg.push_back(static_cast<uint8_t>(status));

        transport.SendFrame(CanPriority::response, CanCategory::system, CanMessageType::commandAck, msg, onDone);
    }

    void CanProtocolServerImpl::ResetRateCounter()
    {
        messageCountThisPeriod = 0;
    }

    bool CanProtocolServerImpl::IsRateLimited() const
    {
        return messageCountThisPeriod >= config.maxMessagesPerSecond;
    }

    bool CanProtocolServerImpl::CheckAndIncrementRate()
    {
        if (messageCountThisPeriod >= config.maxMessagesPerSecond)
            return false;

        ++messageCountThisPeriod;
        return true;
    }

    bool CanProtocolServerImpl::ValidateSequence(uint8_t sequenceNumber)
    {
        if (!sequenceInitialized)
        {
            sequenceInitialized = true;
            lastSequenceNumber = sequenceNumber;
            return true;
        }

        auto expected = static_cast<uint8_t>(lastSequenceNumber + 1);
        if (sequenceNumber != expected)
            return false;

        lastSequenceNumber = sequenceNumber;
        return true;
    }
}
