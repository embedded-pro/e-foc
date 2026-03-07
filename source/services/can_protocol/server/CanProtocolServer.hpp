#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolServer;

    class CanProtocolServerObserver
        : public infra::SingleObserver<CanProtocolServerObserver, CanProtocolServer>
    {
    public:
        using infra::SingleObserver<CanProtocolServerObserver, CanProtocolServer>::SingleObserver;

        virtual void OnMotorStart() = 0;
        virtual void OnMotorStop() = 0;
        virtual void OnEmergencyStop() = 0;
        virtual void OnControlModeChanged(CanControlMode mode) = 0;
        virtual void OnTorqueSetpoint(float idCurrent, float iqCurrent) = 0;
        virtual void OnSpeedSetpoint(float speedRadPerSec) = 0;
        virtual void OnPositionSetpoint(float positionRad) = 0;

        virtual void OnCurrentIdPidChanged(float kp, float ki, float kd) = 0;
        virtual void OnCurrentIqPidChanged(float kp, float ki, float kd) = 0;
        virtual void OnSpeedPidChanged(float kp, float ki, float kd) = 0;
        virtual void OnPositionPidChanged(float kp, float ki, float kd) = 0;

        virtual void OnSupplyVoltageChanged(float voltageVolts) = 0;
        virtual void OnMaxCurrentChanged(float currentAmps) = 0;

        virtual void OnDataRequested(DataRequestFlags flags, DataResponse& response) = 0;
    };

    class CanProtocolServer
        : public infra::Subject<CanProtocolServerObserver>
    {
    public:
        struct Config
        {
            uint16_t nodeId;
            uint16_t maxMessagesPerSecond;
        };

        virtual void SendCommandAck(CanCategory category, CanMessageType commandType, CanAckStatus status, const infra::Function<void()>& onDone) = 0;

        void SendCommandAck(CanCategory category, CanMessageType commandType, CanAckStatus status)
        {
            SendCommandAck(category, commandType, status, [] {});
        }

        virtual void HandleDataRequest(DataRequestFlags flags) = 0;

        virtual void ResetRateCounter() = 0;
        virtual bool IsRateLimited() const = 0;
    };
}
