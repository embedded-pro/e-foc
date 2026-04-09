#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
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
        void OnMotorStatus(services::FocMotorState state, services::FocFaultCode fault);
        void OnCurrentMeasurement(float idCurrent, float iqCurrent);
        void OnSpeedPosition(float speed, float position);
        void OnBusVoltage(float voltage);
        void OnFaultEvent(services::FocFaultCode fault);

    private:
        static QString MotorStateName(services::FocMotorState state);
        static QString FaultCodeName(services::FocFaultCode fault);

        QLabel* motorStateLabel;
        QLabel* faultLabel;
        QLabel* idCurrentLabel;
        QLabel* iqCurrentLabel;
        QLabel* speedLabel;
        QLabel* positionLabel;
        QLabel* busVoltageLabel;
    };
}
