#pragma once

#include "infra/util/Observer.hpp"
#include "source/services/can_protocol/CanFrameCodec.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include <cstdint>

namespace tool
{
    class CanCommandClient;

    class CanCommandClientObserver
        : public infra::SingleObserver<CanCommandClientObserver, CanCommandClient>
    {
    public:
        using infra::SingleObserver<CanCommandClientObserver, CanCommandClient>::SingleObserver;

        virtual void OnCommandAckReceived(services::CanCategory category, services::CanMessageType command, services::CanAckStatus status) = 0;
        virtual void OnCommandTimeout() = 0;
        virtual void OnBusyChanged(bool busy) = 0;

        virtual void OnMotorStatusReceived(services::CanMotorState state, services::CanControlMode mode, services::CanFaultCode fault) = 0;
        virtual void OnControlModeReceived(services::CanControlMode mode) = 0;
        virtual void OnCurrentMeasurementReceived(float idCurrent, float iqCurrent) = 0;
        virtual void OnSpeedPositionReceived(float speed, float position) = 0;
        virtual void OnBusVoltageReceived(float voltage) = 0;
        virtual void OnFaultEventReceived(services::CanFaultCode fault) = 0;
        virtual void OnHeartbeatReceived(uint8_t protocolVersion) = 0;

        virtual void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) = 0;

        virtual void OnConnectionChanged(bool connected) = 0;
        virtual void OnAdapterError(infra::BoundedConstString message) = 0;
    };

    class CanCommandClient
        : public infra::Subject<CanCommandClientObserver>
        , private CanBusAdapterObserver
    {
    public:
        explicit CanCommandClient(CanBusAdapter& adapter);

        void SetNodeId(uint16_t nodeId);
        uint16_t NodeId() const;
        bool IsBusy() const;

        void SendStartMotor();
        void SendStopMotor();
        void SendEmergencyStop();
        void SendSetControlMode(services::CanControlMode mode);
        void SendSetTorqueSetpoint(float idCurrent, float iqCurrent);
        void SendSetSpeedSetpoint(float speedRadPerSec);
        void SendSetPositionSetpoint(float positionRad);

        void SendSetCurrentIdPid(float kp, float ki, float kd);
        void SendSetCurrentIqPid(float kp, float ki, float kd);
        void SendSetSpeedPid(float kp, float ki, float kd);
        void SendSetPositionPid(float kp, float ki, float kd);

        void SendSetPolePairs(uint8_t polePairs);
        void SendSetResistance(float ohms);
        void SendSetInductance(float henries);
        void SendSetFluxLinkage(float webers);

        void SendSetSupplyVoltage(float volts);
        void SendSetMaxCurrent(float amps);

        void SendHeartbeat();
        void SendRequestStatus();

        void HandleTimeout();

    private:
        void OnFrameReceived(uint32_t id, const CanFrame& data) override;
        void OnError(infra::BoundedConstString message) override;
        void OnConnectionChanged(bool connected) override;

        void SendCommand(services::CanPriority priority, services::CanCategory category,
            services::CanMessageType type, const CanFrame& payload);
        void SetBusy(bool busy);

        void HandleTelemetry(services::CanMessageType type, const CanFrame& data);
        void HandleSystemMessage(services::CanMessageType type, const CanFrame& data);

        uint8_t NextSequence();

        CanBusAdapter& adapter;
        uint16_t nodeId = 1;
        uint8_t sequenceCounter = 0;
        bool busy = false;
    };
}
