#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "core/can/FocMotorWireContract.hpp"

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
        focClient.SetNodeId(id);
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

    template<class Send>
    bool CanCommandClient::Tracked(Send send)
    {
        SetBusy(true);
        const bool sent = send();
        if (!sent)
            SetBusy(false);
        return sent;
    }

    bool CanCommandClient::SendStartMotor()
    {
        return Tracked([&]()
            {
                return focClient.Start();
            });
    }

    bool CanCommandClient::SendStopMotor()
    {
        return Tracked([&]()
            {
                return focClient.Stop();
            });
    }

    bool CanCommandClient::SendEmergencyStop()
    {
        return Tracked([&]()
            {
                return focClient.EmergencyStop();
            });
    }

    bool CanCommandClient::SendSetControlMode(can::FocMotorMode mode)
    {
        return Tracked([&]()
            {
                return focClient.SelectControlMode(mode);
            });
    }

    bool CanCommandClient::SendSetTorqueSetpoint(float iqCurrent)
    {
        return Tracked([&]()
            {
                return focClient.SetTorque(foc::Ampere{ iqCurrent });
            });
    }

    bool CanCommandClient::SendSetSpeedSetpoint(float speedRadPerSec)
    {
        return Tracked([&]()
            {
                return focClient.SetSpeed(foc::RadiansPerSecond{ speedRadPerSec });
            });
    }

    bool CanCommandClient::SendSetPositionSetpoint(float positionRad)
    {
        return Tracked([&]()
            {
                return focClient.SetPosition(foc::Radians{ positionRad });
            });
    }

    bool CanCommandClient::SendSetCurrentBandwidth(float bandwidth)
    {
        return Tracked([&]()
            {
                return focClient.SetCurrentBandwidth(bandwidth);
            });
    }

    bool CanCommandClient::SendSetSpeedBandwidth(float bandwidth)
    {
        return Tracked([&]()
            {
                return focClient.SetSpeedBandwidth(bandwidth);
            });
    }

    bool CanCommandClient::SendSetPositionBandwidth(float bandwidth)
    {
        return Tracked([&]()
            {
                return focClient.SetPositionBandwidth(bandwidth);
            });
    }

    void CanCommandClient::RequestData()
    {
        focClient.QueryContractVersion();
    }

    void CanCommandClient::HandleTimeout()
    {
        SetBusy(false);
        NotifyObservers([](auto& observer)
            {
                observer.OnCommandTimeout();
            });
    }

    void CanCommandClient::OnServerOnline(uint16_t serverNodeId)
    {
        if (serverNodeId != nodeId)
            return;

        RequestData();

        NotifyObservers([](auto& observer)
            {
                observer.OnNodeOnlineChanged(true);
            });
    }

    void CanCommandClient::OnServerOffline(uint16_t serverNodeId)
    {
        if (serverNodeId != nodeId)
            return;

        NotifyObservers([](auto& observer)
            {
                observer.OnNodeOnlineChanged(false);
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
        if (!can::PayloadExact(msg, can::focTelemetryStatusResponseId))
            return;

        services::CanPayloadReader reader{ msg };
        const auto state = static_cast<FocMotorState>(reader.ReadUInt8());
        const auto fault = static_cast<FocFaultCode>(reader.ReadUInt8());
        const auto speed = reader.ReadFixed16(can::focSpeedScale);
        const auto position = reader.ReadFixed16(can::focPositionScale);

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
        if (!can::PayloadExact(msg, can::focTelemetryElectricalResponseId))
            return;

        services::CanPayloadReader reader{ msg };
        const auto voltage = reader.ReadFixed16(can::focVoltageScale);
        reader.Skip(2);
        const auto iq = reader.ReadFixed16(can::focCurrentScale);
        const auto id = reader.ReadFixed16(can::focCurrentScale);

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
            if (msgType == can::focTelemetryStatusResponseId || msgType == can::focTelemetryElectricalResponseId)
                consumed = true;
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

    void CanCommandClient::OnElectricalParamsResponse(foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs)
    {
        NotifyObservers([resistance, inductance, polePairs](auto& observer)
            {
                observer.OnElectricalParametersReceived(resistance.Value(), inductance.Value(), polePairs);
            });
    }

    void CanCommandClient::OnMechanicalParamsResponse(foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia)
    {
        NotifyObservers([friction, inertia](auto& observer)
            {
                observer.OnMechanicalParametersReceived(friction.Value(), inertia.Value());
            });
    }

    void CanCommandClient::OnContractVersionResponse(uint8_t major, uint8_t minor)
    {
        const bool compatible = major == can::focContractVersionMajor;

        NotifyObservers([major, minor, compatible](auto& observer)
            {
                observer.OnContractVersion(major, minor, compatible);
            });
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
