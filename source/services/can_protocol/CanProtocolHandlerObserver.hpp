#pragma once

#include "infra/util/Observer.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"

namespace services
{
    class CanProtocolHandler;

    class CanProtocolHandlerObserver
        : public infra::SingleObserver<CanProtocolHandlerObserver, CanProtocolHandler>
    {
    public:
        using infra::SingleObserver<CanProtocolHandlerObserver, CanProtocolHandler>::SingleObserver;

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

        virtual void OnPolePairsChanged(uint8_t polePairs) = 0;
        virtual void OnResistanceChanged(float resistanceOhm) = 0;
        virtual void OnInductanceChanged(float inductanceHenry) = 0;
        virtual void OnFluxLinkageChanged(float fluxWeber) = 0;

        virtual void OnSupplyVoltageChanged(float voltageVolts) = 0;
        virtual void OnMaxCurrentChanged(float currentAmps) = 0;

        virtual void OnHeartbeatReceived(uint8_t protocolVersion) = 0;
    };
}
