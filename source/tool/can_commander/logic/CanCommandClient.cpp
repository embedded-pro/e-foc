#include "source/tool/can_commander/logic/CanCommandClient.hpp"

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(CanBusAdapter& adapter)
        : adapter(adapter)
        , protocolClient(adapter, CanProtocolClient::Config{ 1 })
    {
        CanProtocolClientObserver::Attach(protocolClient);
        CanBusAdapterObserver::Attach(adapter);
    }

    CanCommandClient::~CanCommandClient()
    {
        CanProtocolClientObserver::Detach();
        CanBusAdapterObserver::Detach();
    }

    void CanCommandClient::SetNodeId(uint16_t id)
    {
        nodeId = id;
        protocolClient.SetNodeId(id);
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
        protocolClient.SendStartMotor([] {});
    }

    void CanCommandClient::SendStopMotor()
    {
        SetBusy(true);
        protocolClient.SendStopMotor([] {});
    }

    void CanCommandClient::SendEmergencyStop()
    {
        SetBusy(true);
        protocolClient.SendEmergencyStop([] {});
    }

    void CanCommandClient::SendSetControlMode(CanControlMode mode)
    {
        SetBusy(true);
        protocolClient.SendSetControlMode(mode, [] {});
    }

    void CanCommandClient::SendSetTorqueSetpoint(float idCurrent, float iqCurrent)
    {
        SetBusy(true);
        protocolClient.SendSetTorqueSetpoint(idCurrent, iqCurrent, [] {});
    }

    void CanCommandClient::SendSetSpeedSetpoint(float speedRadPerSec)
    {
        SetBusy(true);
        protocolClient.SendSetSpeedSetpoint(speedRadPerSec, [] {});
    }

    void CanCommandClient::SendSetPositionSetpoint(float positionRad)
    {
        SetBusy(true);
        protocolClient.SendSetPositionSetpoint(positionRad, [] {});
    }

    void CanCommandClient::SendSetCurrentIdPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        protocolClient.SendSetCurrentIdPid(kp, ki, kd, [] {});
    }

    void CanCommandClient::SendSetCurrentIqPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        protocolClient.SendSetCurrentIqPid(kp, ki, kd, [] {});
    }

    void CanCommandClient::SendSetSpeedPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        protocolClient.SendSetSpeedPid(kp, ki, kd, [] {});
    }

    void CanCommandClient::SendSetPositionPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        protocolClient.SendSetPositionPid(kp, ki, kd, [] {});
    }

    void CanCommandClient::SendSetSupplyVoltage(float volts)
    {
        SetBusy(true);
        protocolClient.SendSetSupplyVoltage(volts, [] {});
    }

    void CanCommandClient::SendSetMaxCurrent(float amps)
    {
        SetBusy(true);
        protocolClient.SendSetMaxCurrent(amps, [] {});
    }

    void CanCommandClient::RequestData(DataRequestFlags flags)
    {
        protocolClient.RequestData(flags, [] {});
    }

    void CanCommandClient::HandleTimeout()
    {
        SetBusy(false);
        NotifyObservers([](auto& observer)
            {
                observer.OnCommandTimeout();
            });
    }

    void CanCommandClient::OnCommandAckReceived(CanCategory category, CanMessageType command, CanAckStatus status)
    {
        SetBusy(false);
        NotifyObservers([category, command, status](auto& observer)
            {
                observer.OnCommandAckReceived(category, command, status);
            });
    }

    void CanCommandClient::OnMotorStatusReceived(CanMotorState state, CanControlMode mode, CanFaultCode fault)
    {
        NotifyObservers([state, mode, fault](auto& observer)
            {
                observer.OnMotorStatusReceived(state, mode, fault);
            });
    }

    void CanCommandClient::OnControlModeReceived(CanControlMode mode)
    {
        NotifyObservers([mode](auto& observer)
            {
                observer.OnControlModeReceived(mode);
            });
    }

    void CanCommandClient::OnCurrentMeasurementReceived(float idCurrent, float iqCurrent)
    {
        NotifyObservers([idCurrent, iqCurrent](auto& observer)
            {
                observer.OnCurrentMeasurementReceived(idCurrent, iqCurrent);
            });
    }

    void CanCommandClient::OnSpeedPositionReceived(float speed, float position)
    {
        NotifyObservers([speed, position](auto& observer)
            {
                observer.OnSpeedPositionReceived(speed, position);
            });
    }

    void CanCommandClient::OnBusVoltageReceived(float voltage)
    {
        NotifyObservers([voltage](auto& observer)
            {
                observer.OnBusVoltageReceived(voltage);
            });
    }

    void CanCommandClient::OnFaultEventReceived(CanFaultCode fault)
    {
        NotifyObservers([fault](auto& observer)
            {
                observer.OnFaultEventReceived(fault);
            });
    }

    void CanCommandClient::OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data)
    {
        NotifyObservers([transmitted, id, &data](auto& observer)
            {
                observer.OnFrameLog(transmitted, id, data);
            });
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
}
