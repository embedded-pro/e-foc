#pragma once

#include "source/services/can_protocol/CanFrameCodec.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include <QObject>
#include <QTimer>

namespace tool
{
    class CanCommandClient : public QObject
    {
        Q_OBJECT

    public:
        explicit CanCommandClient(CanBusAdapter& adapter, QObject* parent = nullptr);

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

    signals:
        void CommandAckReceived(services::CanCategory category, services::CanMessageType command, services::CanAckStatus status);
        void CommandTimeout();
        void BusyChanged(bool busy);

        void MotorStatusReceived(services::CanMotorState state, services::CanControlMode mode, services::CanFaultCode fault);
        void ControlModeReceived(services::CanControlMode mode);
        void CurrentMeasurementReceived(float idCurrent, float iqCurrent);
        void SpeedPositionReceived(float speed, float position);
        void BusVoltageReceived(float voltage);
        void FaultEventReceived(services::CanFaultCode fault);
        void HeartbeatReceived(uint8_t protocolVersion);

        void FrameSent(uint32_t id, QByteArray data);
        void FrameLog(QString direction, uint32_t id, QByteArray data);

    private:
        void OnFrameReceived(uint32_t id, QByteArray data);
        void OnTimeout();
        void SendCommand(services::CanPriority priority, services::CanCategory category,
            services::CanMessageType type, const QByteArray& payload);
        void SetBusy(bool busy);

        void HandleTelemetry(services::CanMessageType type, const QByteArray& data);
        void HandleSystemMessage(services::CanMessageType type, const QByteArray& data);

        uint8_t NextSequence();

        CanBusAdapter& adapter;
        QTimer timeoutTimer;
        uint16_t nodeId = 1;
        uint8_t sequenceCounter = 0;
        bool busy = false;

        static constexpr int commandTimeoutMs = 2000;
    };
}
