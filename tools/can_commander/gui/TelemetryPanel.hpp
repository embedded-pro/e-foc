#pragma once

#include "tools/can_commander/logic/CanCommandClient.hpp"
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
        void OnMotorStatus(FocMotorState state, FocFaultCode fault);
        void OnCurrentMeasurement(float idCurrent, float iqCurrent);
        void OnSpeedPosition(float speed, float position);
        void OnBusVoltage(float voltage);
        void OnFaultEvent(FocFaultCode fault);

    private:
        static QString MotorStateName(FocMotorState state);
        static QString FaultCodeName(FocFaultCode fault);

        QLabel* motorStateLabel;
        QLabel* faultLabel;
        QLabel* idCurrentLabel;
        QLabel* iqCurrentLabel;
        QLabel* speedLabel;
        QLabel* positionLabel;
        QLabel* busVoltageLabel;
    };
}
