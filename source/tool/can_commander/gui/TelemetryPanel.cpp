#include "source/tool/can_commander/gui/TelemetryPanel.hpp"
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
        controlModeLabel = new QLabel("---");
        faultLabel = new QLabel("---");
        statusLayout->addRow("State:", motorStateLabel);
        statusLayout->addRow("Mode:", controlModeLabel);
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

        auto* systemGroup = new QGroupBox("System");
        auto* systemLayout = new QFormLayout(systemGroup);
        heartbeatLabel = new QLabel("---");
        lastAckLabel = new QLabel("---");
        systemLayout->addRow("Heartbeat:", heartbeatLabel);
        systemLayout->addRow("Last Ack:", lastAckLabel);
        layout->addWidget(systemGroup);

        layout->addStretch();
    }

    void TelemetryPanel::OnMotorStatus(CanMotorState state, CanControlMode mode, CanFaultCode fault)
    {
        motorStateLabel->setText(MotorStateName(state));
        controlModeLabel->setText(ControlModeName(mode));
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

    void TelemetryPanel::OnFaultEvent(CanFaultCode fault)
    {
        faultLabel->setText(FaultCodeName(fault));
        faultLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    void TelemetryPanel::OnHeartbeat(uint8_t protocolVersion)
    {
        heartbeatLabel->setText("v" + QString::number(protocolVersion));
    }

    void TelemetryPanel::OnCommandAck(CanCategory category, CanMessageType command, CanAckStatus status)
    {
        QString text = CategoryName(category) + " / 0x" +
                       QString::number(static_cast<uint8_t>(command), 16).toUpper() +
                       " → " + AckStatusName(status);

        lastAckLabel->setText(text);

        if (status == CanAckStatus::success)
            lastAckLabel->setStyleSheet("color: green;");
        else
            lastAckLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    QString TelemetryPanel::MotorStateName(CanMotorState state)
    {
        switch (state)
        {
            case CanMotorState::idle:
                return "Idle";
            case CanMotorState::running:
                return "Running";
            case CanMotorState::fault:
                return "Fault";
            case CanMotorState::aligning:
                return "Aligning";
            default:
                return "Unknown";
        }
    }

    QString TelemetryPanel::ControlModeName(CanControlMode mode)
    {
        switch (mode)
        {
            case CanControlMode::torque:
                return "Torque";
            case CanControlMode::speed:
                return "Speed";
            case CanControlMode::position:
                return "Position";
            default:
                return "Unknown";
        }
    }

    QString TelemetryPanel::FaultCodeName(CanFaultCode fault)
    {
        switch (fault)
        {
            case CanFaultCode::none:
                return "None";
            case CanFaultCode::overCurrent:
                return "Over Current";
            case CanFaultCode::overVoltage:
                return "Over Voltage";
            case CanFaultCode::overTemperature:
                return "Over Temperature";
            case CanFaultCode::sensorFault:
                return "Sensor Fault";
            case CanFaultCode::communicationTimeout:
                return "Communication Timeout";
            default:
                return "Unknown";
        }
    }

    QString TelemetryPanel::AckStatusName(CanAckStatus status)
    {
        switch (status)
        {
            case CanAckStatus::success:
                return "Success";
            case CanAckStatus::unknownCommand:
                return "Unknown Command";
            case CanAckStatus::invalidPayload:
                return "Invalid Payload";
            case CanAckStatus::invalidState:
                return "Invalid State";
            case CanAckStatus::sequenceError:
                return "Sequence Error";
            case CanAckStatus::rateLimited:
                return "Rate Limited";
            default:
                return "Unknown";
        }
    }

    QString TelemetryPanel::CategoryName(CanCategory category)
    {
        switch (category)
        {
            case CanCategory::motorControl:
                return "Motor Control";
            case CanCategory::pidTuning:
                return "PID Tuning";
            case CanCategory::motorParameters:
                return "Motor Params";
            case CanCategory::systemParameters:
                return "System Params";
            case CanCategory::telemetry:
                return "Telemetry";
            case CanCategory::system:
                return "System";
            default:
                return "Unknown";
        }
    }
}
