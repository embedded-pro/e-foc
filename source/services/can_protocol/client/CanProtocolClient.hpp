#pragma once

#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolClient;

    class CanProtocolClientObserver
        : public infra::SingleObserver<CanProtocolClientObserver, CanProtocolClient>
    {
    public:
        using infra::SingleObserver<CanProtocolClientObserver, CanProtocolClient>::SingleObserver;

        virtual void OnCommandAckReceived(CanCategory category, CanMessageType command, CanAckStatus status) = 0;
        virtual void OnMotorStatusReceived(CanMotorState state, CanControlMode mode, CanFaultCode fault) = 0;
        virtual void OnControlModeReceived(CanControlMode mode) = 0;
        virtual void OnCurrentMeasurementReceived(float idCurrent, float iqCurrent) = 0;
        virtual void OnSpeedPositionReceived(float speed, float position) = 0;
        virtual void OnBusVoltageReceived(float voltage) = 0;
        virtual void OnFaultEventReceived(CanFaultCode fault) = 0;
    };

    class CanProtocolClient
        : public infra::Subject<CanProtocolClientObserver>
    {
    public:
        struct Config
        {
            uint16_t nodeId = 1;
        };

        virtual void SetNodeId(uint16_t nodeId) = 0;
        virtual uint16_t NodeId() const = 0;

        virtual void SendStartMotor(const infra::Function<void()>& onDone) = 0;
        virtual void SendStopMotor(const infra::Function<void()>& onDone) = 0;
        virtual void SendEmergencyStop(const infra::Function<void()>& onDone) = 0;
        virtual void SendSetControlMode(CanControlMode mode, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetTorqueSetpoint(float idCurrent, float iqCurrent, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetSpeedSetpoint(float speedRadPerSec, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetPositionSetpoint(float positionRad, const infra::Function<void()>& onDone) = 0;

        virtual void SendSetCurrentIdPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetCurrentIqPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetSpeedPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetPositionPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) = 0;

        virtual void SendSetSupplyVoltage(float volts, const infra::Function<void()>& onDone) = 0;
        virtual void SendSetMaxCurrent(float amps, const infra::Function<void()>& onDone) = 0;

        virtual void RequestData(DataRequestFlags flags, const infra::Function<void()>& onDone) = 0;
    };
}
