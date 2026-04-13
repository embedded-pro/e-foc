#include "tools/can_commander/logic/CanCommandClient.hpp"
#include <algorithm>
#include <limits>

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(CanBusAdapter& adapter)
        : focTransport(adapter, 0)
        , protocolClient(adapter)
        , focCategory(focTransport, protocolClient)
    {
        protocolClient.RegisterCategory(focCategory);
        CanProtocolClientObserver::Attach(protocolClient);
        FocMotorCategoryClientObserver::Attach(focCategory);
        CanBusAdapterObserver::Attach(adapter);
    }

    CanCommandClient::~CanCommandClient()
    {
        FocMotorCategoryClientObserver::Detach();
        CanProtocolClientObserver::Detach();
        CanBusAdapterObserver::Detach();
        protocolClient.UnregisterCategory(focCategory);
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
        if (focCategory.SendStart(nodeId))
            SetBusy(false);
    }

    void CanCommandClient::SendStopMotor()
    {
        SetBusy(true);
        if (focCategory.SendStop(nodeId))
            SetBusy(false);
    }

    void CanCommandClient::SendEmergencyStop()
    {
        SetBusy(true);
        if (focCategory.SendEmergencyStop(nodeId))
            SetBusy(false);
    }

    void CanCommandClient::SendSetControlMode(FocMotorMode mode)
    {
        SetBusy(true);
        if (focCategory.SendSetTarget(nodeId, FocSetpoint{ mode, 0 }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetTorqueSetpoint(float iqCurrent)
    {
        SetBusy(true);
        if (focCategory.SendSetTarget(nodeId, FocSetpoint{ FocMotorMode::torque, static_cast<int16_t>(iqCurrent * focCurrentScale) }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetSpeedSetpoint(float speedRadPerSec)
    {
        SetBusy(true);
        const auto scaled = std::clamp(static_cast<int32_t>(speedRadPerSec * focSpeedScale),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
        if (focCategory.SendSetTarget(nodeId, FocSetpoint{ FocMotorMode::speed, static_cast<int16_t>(scaled) }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetPositionSetpoint(float positionRad)
    {
        SetBusy(true);
        const auto scaled = std::clamp(static_cast<int32_t>(positionRad * focPositionScale),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
        if (focCategory.SendSetTarget(nodeId, FocSetpoint{ FocMotorMode::position, static_cast<int16_t>(scaled) }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetCurrentIdPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        if (focCategory.SendSetPidCurrent(nodeId, FocPidGains{ static_cast<int16_t>(kp * focPidScale), static_cast<int16_t>(ki * focPidScale), static_cast<int16_t>(kd * focPidScale) }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetCurrentIqPid(float kp, float ki, float kd)
    {
        SendSetCurrentIdPid(kp, ki, kd);
    }

    void CanCommandClient::SendSetSpeedPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        if (focCategory.SendSetPidSpeed(nodeId, FocPidGains{ static_cast<int16_t>(kp * focPidScale), static_cast<int16_t>(ki * focPidScale), static_cast<int16_t>(kd * focPidScale) }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetPositionPid(float kp, float ki, float kd)
    {
        SetBusy(true);
        if (focCategory.SendSetPidPosition(nodeId, FocPidGains{ static_cast<int16_t>(kp * focPidScale), static_cast<int16_t>(ki * focPidScale), static_cast<int16_t>(kd * focPidScale) }))
            SetBusy(false);
    }

    void CanCommandClient::SendSetSupplyVoltage(float /*volts*/) const
    {
        // No equivalent in can-lite FocMotorCategoryClient; intentional no-op
    }

    void CanCommandClient::SendSetMaxCurrent(float /*amps*/) const
    {
        // No equivalent in can-lite FocMotorCategoryClient; intentional no-op
    }

    void CanCommandClient::RequestData()
    {
        focCategory.SendRequestTelemetry(nodeId);
    }

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

    void CanCommandClient::OnMotorTypeResponse(FocMotorMode /*mode*/) {}

    void CanCommandClient::OnElectricalParamsResponse(const FocElectricalParams& /*params*/) {}

    void CanCommandClient::OnMechanicalParamsResponse(const FocMechanicalParams& /*params*/) {}

    void CanCommandClient::OnTelemetryStatusResponse(const FocTelemetryStatus& status)
    {
        NotifyObservers([&status](auto& observer)
            {
                observer.OnMotorStatusReceived(status.state, status.fault);
            });

        const float speed = static_cast<float>(status.speed) / focSpeedScale;
        const float position = static_cast<float>(status.position) / focPositionScale;
        NotifyObservers([speed, position](auto& observer)
            {
                observer.OnSpeedPositionReceived(speed, position);
            });

        if (status.fault != FocFaultCode::none)
        {
            NotifyObservers([&status](auto& observer)
                {
                    observer.OnFaultEventReceived(status.fault);
                });
        }
    }

    void CanCommandClient::OnTelemetryElectricalResponse(const FocTelemetryElectrical& telemetry)
    {
        const float id = static_cast<float>(telemetry.id) / focCurrentScale;
        const float iq = static_cast<float>(telemetry.iq) / focCurrentScale;
        NotifyObservers([id, iq](auto& observer)
            {
                observer.OnCurrentMeasurementReceived(id, iq);
            });

        const float voltage = static_cast<float>(telemetry.voltage) / focVoltageScale;
        NotifyObservers([voltage](auto& observer)
            {
                observer.OnBusVoltageReceived(voltage);
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

