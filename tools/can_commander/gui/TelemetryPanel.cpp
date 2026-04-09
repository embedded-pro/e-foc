#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include <QFormLayout>
#include <QVBoxLayout>

namespace tool
{
    using namespace services;

    TelemetryPanel::TelemetryPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);

        auto* statusGroup = new QGroupBox("Motor Status");
        auto* statusLayout = new QFormLayout(statusGroup);
        motorStateLabel = new QLabel("---");
        faultLabel = new QLabel("---");
        statusLayout->addRow("State:", motorStateLabel);
        statusLayout->addRow("Fault:", faultLabel);
        layout->addWidget(statusGroup);

        auto* measureGroup = new QGroupBox("Measurements");
        auto* measureLayout = new QFormLayout(measureGroup);
        idCurrentLabel = new QLabel("--- A");
        iqCurrentLabel = new QLabel("--- A");
        speedLabel = new QLabel("--- rad/s");
        positionLabel = new QLabel("--- rad");
        busVoltageLabel = new QLabel("--- V");
        measureLayout->addRow("Id Current:", idCurrentLabel);
        measureLayout->addRow("Iq Current:", iqCurrentLabel);
        measureLayout->addRow("Speed:", speedLabel);
        measureLayout->addRow("Position:", positionLabel);
        measureLayout->addRow("Bus Voltage:", busVoltageLabel);
        layout->addWidget(measureGroup);

        layout->addStretch();
    }

    void TelemetryPanel::OnMotorStatus(FocMotorState state, FocFaultCode fault)
    {
        motorStateLabel->setText(MotorStateName(state));
        faultLabel->setText(FaultCodeName(fault));
    }

    void TelemetryPanel::OnCurrentMeasurement(float idCurrent, float iqCurrent)
    {
        idCurrentLabel->setText(QString::number(idCurrent, 'f', 3) + " A");
        iqCurrentLabel->setText(QString::number(iqCurrent, 'f', 3) + " A");
    }

    void TelemetryPanel::OnSpeedPosition(float speed, float position)
    {
        speedLabel->setText(QString::number(speed, 'f', 3) + " rad/s");
        positionLabel->setText(QString::number(position, 'f', 4) + " rad");
    }

    void TelemetryPanel::OnBusVoltage(float voltage)
    {
        busVoltageLabel->setText(QString::number(voltage, 'f', 2) + " V");
    }

    void TelemetryPanel::OnFaultEvent(FocFaultCode fault)
    {
        faultLabel->setText(FaultCodeName(fault));
        faultLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    QString TelemetryPanel::MotorStateName(FocMotorState state)
    {
        switch (state)
        {
            case FocMotorState::idle:
                return "Idle";
            case FocMotorState::running:
                return "Running";
            case FocMotorState::fault:
                return "Fault";
            case FocMotorState::calibrating:
                return "Calibrating";
            default:
                return "Unknown";
        }
    }

    QString TelemetryPanel::FaultCodeName(FocFaultCode fault)
    {
        switch (fault)
        {
            case FocFaultCode::none:
                return "None";
            case FocFaultCode::overCurrent:
                return "Over Current";
            case FocFaultCode::overVoltage:
                return "Over Voltage";
            case FocFaultCode::underVoltage:
                return "Under Voltage";
            case FocFaultCode::overTemperature:
                return "Over Temperature";
            case FocFaultCode::sensorFault:
                return "Sensor Fault";
            default:
                return "Unknown";
        }
    }
}
