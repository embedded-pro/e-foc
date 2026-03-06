#pragma once

#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include <QGroupBox>
#include <QLabel>
#include <QWidget>

namespace tool
{
    class TelemetryPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit TelemetryPanel(QWidget* parent = nullptr);

    public slots:
        void OnMotorStatus(services::CanMotorState state, services::CanControlMode mode, services::CanFaultCode fault);
        void OnCurrentMeasurement(float idCurrent, float iqCurrent);
        void OnSpeedPosition(float speed, float position);
        void OnBusVoltage(float voltage);
        void OnFaultEvent(services::CanFaultCode fault);
        void OnHeartbeat(uint8_t protocolVersion);
        void OnCommandAck(services::CanCategory category, services::CanMessageType command, services::CanAckStatus status);

    private:
        static QString MotorStateName(services::CanMotorState state);
        static QString ControlModeName(services::CanControlMode mode);
        static QString FaultCodeName(services::CanFaultCode fault);
        static QString AckStatusName(services::CanAckStatus status);
        static QString CategoryName(services::CanCategory category);

        QLabel* motorStateLabel;
        QLabel* controlModeLabel;
        QLabel* faultLabel;
        QLabel* idCurrentLabel;
        QLabel* iqCurrentLabel;
        QLabel* speedLabel;
        QLabel* positionLabel;
        QLabel* busVoltageLabel;
        QLabel* heartbeatLabel;
        QLabel* lastAckLabel;
    };
}
