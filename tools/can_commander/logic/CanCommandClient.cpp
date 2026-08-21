#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanPayload.hpp"
#include <algorithm>
#include <limits>

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(CanBusAdapter& adapter)
        : focClient{ adapter, nodeId }
    {
        CanProtocolClientObserver::Attach(focClient.ProtocolClient());
        can::FocMotorCategoryClientObserver::Attach(focClient.CategoryClient());
        CanBusAdapterObserver::Attach(adapter);
    }

    CanCommandClient::~CanCommandClient()
    {
        can::FocMotorCategoryClientObserver::Detach();
        CanProtocolClientObserver::Detach();
        CanBusAdapterObserver::Detach();
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

    void CanCommandClient::SendStartMotor()
    {
        SetBusy(true);
        if (focClient.Start())
            SetBusy(false);
    }

    void CanCommandClient::SendStopMotor()
    {
        SetBusy(true);
        if (focClient.Stop())
            SetBusy(false);
    }

    void CanCommandClient::SendEmergencyStop()
    {
        SetBusy(true);
        if (focClient.EmergencyStop())
            SetBusy(false);
    }

    void CanCommandClient::SendSetControlMode(can::FocMotorMode mode)
    {
        SetBusy(true);
        if (focClient.SelectControlMode(mode))
            SetBusy(false);
    }

    void CanCommandClient::SendSetTorqueSetpoint(float iqCurrent)
    {
        SetBusy(true);
        const auto scaled = std::clamp(static_cast<int32_t>(iqCurrent * can::focCurrentScale),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
        if (focClient.SetTorque(foc::Ampere{ static_cast<float>(scaled) / can::focCurrentScale }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetSpeedSetpoint(float speedRadPerSec)
    {
        SetBusy(true);
        const auto scaled = std::clamp(static_cast<int32_t>(speedRadPerSec * can::focSpeedScale),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
        if (focClient.SetSpeed(foc::RadiansPerSecond{ static_cast<float>(scaled) / can::focSpeedScale }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetPositionSetpoint(float positionRad)
    {
        SetBusy(true);
        const auto scaled = std::clamp(static_cast<int32_t>(positionRad * can::focPositionScale),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
        if (focClient.SetPosition(foc::Radians{ static_cast<float>(scaled) / can::focPositionScale }))
            SetBusy(false);
    }

    void CanCommandClient::CompleteImmediately()
    {
        SetBusy(true);
        SetBusy(false);
    }

    void CanCommandClient::SendSetCurrentIdPid(float /*kp*/, float /*ki*/, float /*kd*/)
    {
        CompleteImmediately();
    }

    void CanCommandClient::SendSetCurrentIqPid(float /*kp*/, float /*ki*/, float /*kd*/)
    {
        CompleteImmediately();
    }

    void CanCommandClient::SendSetSpeedPid(float /*kp*/, float /*ki*/, float /*kd*/)
    {
        CompleteImmediately();
    }

    void CanCommandClient::SendSetPositionPid(float /*kp*/, float /*ki*/, float /*kd*/)
    {
        CompleteImmediately();
    }

    void CanCommandClient::RequestData() const
    {}

    void CanCommandClient::HandleTimeout()
    {
        SetBusy(false);
        NotifyObservers([](auto& observer)
            {
                observer.OnCommandTimeout();
            });
    }

    void CanCommandClient::OnServerOnline(uint16_t /*nodeId*/)
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
    }

    void CanCommandClient::OnServerOffline(uint16_t /*nodeId*/)
    {
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    void CanCommandClient::OnCommandAckTimeout(uint16_t /*nodeId*/, uint8_t /*category*/, uint8_t /*messageType*/)
    {}

    void CanCommandClient::OnSelectControlModeResponse(can::FocMotorMode activeMode)
    {
        SetBusy(false);
        NotifyObservers([activeMode](auto& observer)
            {
                observer.OnControlModeAcknowledged(activeMode);
            });
    }

    void CanCommandClient::OnCategoryError(uint8_t /*originCommandId*/, can::FocMotorCategoryError /*errorCode*/)
    {}

    void CanCommandClient::HandleCommandAck(uint8_t categoryId, uint8_t commandType, CanAckStatus status)
    {
        SetBusy(false);
        NotifyObservers([categoryId, commandType, status](auto& observer)
            {
                observer.OnCommandAck(categoryId, commandType, status);
            });
    }

    void CanCommandClient::DecodeTelemetryStatus(const hal::Can::Message& msg) const
    {
        if (msg.size() < 6)
            return;
        const auto state = static_cast<FocMotorState>(msg[0]);
        const auto fault = static_cast<FocFaultCode>(msg[1]);
        const auto speedWire = static_cast<int16_t>((static_cast<uint16_t>(msg[2]) << 8) | msg[3]);
        const auto posWire = static_cast<int16_t>((static_cast<uint16_t>(msg[4]) << 8) | msg[5]);
        const float speed = static_cast<float>(speedWire) / can::focSpeedScale;
        const float position = static_cast<float>(posWire) / can::focPositionScale;

        NotifyObservers([state, fault](auto& observer)
            {
                observer.OnMotorStatusReceived(state, fault);
            });
        NotifyObservers([speed, position](auto& observer)
            {
                observer.OnSpeedPositionReceived(speed, position);
            });
        if (fault != FocFaultCode::none)
        {
            NotifyObservers([fault](auto& observer)
                {
                    observer.OnFaultEventReceived(fault);
                });
        }
    }

    void CanCommandClient::DecodeTelemetryElectrical(const hal::Can::Message& msg) const
    {
        if (msg.size() < 8)
            return;
        const auto voltWire = static_cast<int16_t>((static_cast<uint16_t>(msg[0]) << 8) | msg[1]);
        const auto iqWire = static_cast<int16_t>((static_cast<uint16_t>(msg[4]) << 8) | msg[5]);
        const auto idWire = static_cast<int16_t>((static_cast<uint16_t>(msg[6]) << 8) | msg[7]);
        const float voltage = static_cast<float>(voltWire) / can::focVoltageScale;
        const float iq = static_cast<float>(iqWire) / can::focCurrentScale;
        const float id = static_cast<float>(idWire) / can::focCurrentScale;

        NotifyObservers([id, iq](auto& observer)
            {
                observer.OnCurrentMeasurementReceived(id, iq);
            });
        NotifyObservers([voltage](auto& observer)
            {
                observer.OnBusVoltageReceived(voltage);
            });
    }

    void CanCommandClient::OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data)
    {
        const uint8_t msgType = ExtractCanMessageType(id);
        const uint8_t category = ExtractCanCategory(id);
        bool consumed = false;
        if (!transmitted)
        {
            hal::Can::Message msg;
            for (auto b : data)
                msg.push_back(b);
            if (msgType == can::focTelemetryStatusResponseId)
            {
                DecodeTelemetryStatus(msg);
                consumed = true;
            }
            else if (msgType == can::focTelemetryElectricalResponseId)
            {
                DecodeTelemetryElectrical(msg);
                consumed = true;
            }
            else if (msgType == canCommandAckMessageTypeId && category == canSystemCategoryId && msg.size() >= canCommandAckSize)
            {
                const uint8_t ackCategory = msg[0];
                const uint8_t ackMsgType = msg[1];
                const auto ackStatus = static_cast<CanAckStatus>(msg[2]);
                HandleCommandAck(ackCategory, ackMsgType, ackStatus);
                consumed = true;
            }
        }

        if (!consumed)
        {
            NotifyObservers([transmitted, id, &data](auto& observer)
                {
                    observer.OnFrameLog(transmitted, id, data);
                });
        }
    }

    void CanCommandClient::OnTelemetryStatus(const hal::Can::Message& msg)
    {
        DecodeTelemetryStatus(msg);
    }

    void CanCommandClient::OnTelemetryElectrical(const hal::Can::Message& msg)
    {
        DecodeTelemetryElectrical(msg);
    }

    void CanCommandClient::OnError(infra::BoundedConstString message)
    {
        NotifyObservers([&message](auto& observer)
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
}
