#include "source/tool/can_commander/gui/CommandPanel.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace tool
{
    using namespace services;

    CommandPanel::CommandPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);

        tabs = new QTabWidget();
        tabs->addTab(CreateMotorControlTab(), "Motor Control");
        tabs->addTab(CreatePidTuningTab(), "PID Tuning");
        tabs->addTab(CreateParametersTab(), "Parameters");
        layout->addWidget(tabs);
    }

    QWidget* CommandPanel::CreateMotorControlTab()
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);

        auto* controlGroup = new QGroupBox("Control");
        auto* controlLayout = new QHBoxLayout(controlGroup);

        auto* startButton = new QPushButton("Start Motor");
        auto* stopButton = new QPushButton("Stop Motor");
        auto* estopButton = new QPushButton("EMERGENCY STOP");
        estopButton->setStyleSheet("background-color: red; color: white; font-weight: bold; padding: 10px;");

        controlLayout->addWidget(startButton);
        controlLayout->addWidget(stopButton);
        controlLayout->addWidget(estopButton);
        layout->addWidget(controlGroup);

        commandButtons.push_back(startButton);
        commandButtons.push_back(stopButton);
        commandButtons.push_back(estopButton);

        connect(startButton, &QPushButton::clicked, this, &CommandPanel::StartMotorRequested);
        connect(stopButton, &QPushButton::clicked, this, &CommandPanel::StopMotorRequested);
        connect(estopButton, &QPushButton::clicked, this, &CommandPanel::EmergencyStopRequested);

        auto* modeGroup = new QGroupBox("Control Mode");
        auto* modeLayout = new QHBoxLayout(modeGroup);
        auto* modeCombo = new QComboBox();
        modeCombo->addItem("Torque", static_cast<int>(FocMotorMode::torque));
        modeCombo->addItem("Speed", static_cast<int>(FocMotorMode::speed));
        modeCombo->addItem("Position", static_cast<int>(FocMotorMode::position));
        auto* modeButton = new QPushButton("Set Mode");
        modeLabel = new QLabel("Active: Unknown");
        modeLabel->setStyleSheet("font-weight: bold; color: #555;");
        modeLayout->addWidget(modeCombo);
        modeLayout->addWidget(modeButton);
        modeLayout->addWidget(modeLabel);
        layout->addWidget(modeGroup);

        commandButtons.push_back(modeButton);
        connect(modeButton, &QPushButton::clicked, [this, modeCombo]()
            {
                emit SetControlModeRequested(static_cast<FocMotorMode>(modeCombo->currentData().toInt()));
            });

        setpointStack = new QStackedWidget();

        // Index 0: Torque setpoint
        auto* torqueGroup = new QGroupBox("Torque Setpoint");
        auto* torqueLayout = new QFormLayout(torqueGroup);
        auto* iqSpin = new QDoubleSpinBox();
        iqSpin->setRange(-32.0, 32.0);
        iqSpin->setDecimals(3);
        iqSpin->setSuffix(" A");
        torqueLayout->addRow("Iq:", iqSpin);
        auto* torqueButton = new QPushButton("Set Torque");
        torqueLayout->addRow(torqueButton);
        setpointStack->addWidget(torqueGroup);

        commandButtons.push_back(torqueButton);
        connect(torqueButton, &QPushButton::clicked, [this, iqSpin]()
            {
                emit SetTorqueSetpointRequested(static_cast<float>(iqSpin->value()));
            });

        // Index 1: Speed setpoint
        auto* speedGroup = new QGroupBox("Speed Setpoint");
        auto* speedLayout = new QFormLayout(speedGroup);
        auto* speedSpin = new QDoubleSpinBox();
        speedSpin->setRange(-32767.0, 32767.0);
        speedSpin->setDecimals(3);
        speedSpin->setSuffix(" rad/s");
        speedLayout->addRow("Speed:", speedSpin);
        auto* speedButton = new QPushButton("Set Speed");
        speedLayout->addRow(speedButton);
        setpointStack->addWidget(speedGroup);

        commandButtons.push_back(speedButton);
        connect(speedButton, &QPushButton::clicked, [this, speedSpin]()
            {
                emit SetSpeedSetpointRequested(static_cast<float>(speedSpin->value()));
            });

        // Index 2: Position setpoint
        auto* posGroup = new QGroupBox("Position Setpoint");
        auto* posLayout = new QFormLayout(posGroup);
        auto* posSpin = new QDoubleSpinBox();
        posSpin->setRange(-327.67, 327.67);
        posSpin->setDecimals(4);
        posSpin->setSuffix(" rad");
        posLayout->addRow("Position:", posSpin);
        auto* posButton = new QPushButton("Set Position");
        posLayout->addRow(posButton);
        setpointStack->addWidget(posGroup);

        commandButtons.push_back(posButton);
        connect(posButton, &QPushButton::clicked, [this, posSpin]()
            {
                emit SetPositionSetpointRequested(static_cast<float>(posSpin->value()));
            });

        layout->addWidget(setpointStack);

        auto* queryButton = new QPushButton("Query Controller Status");
        layout->addWidget(queryButton);
        commandButtons.push_back(queryButton);
        connect(queryButton, &QPushButton::clicked, this, &CommandPanel::RequestDataRequested);

        layout->addStretch();
        return page;
    }

    QWidget* CommandPanel::CreatePidTuningTab()
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);

        pidStack = new QStackedWidget();

        // Index 0: Torque mode — Current Id + Iq PID
        auto* torquePidPage = new QWidget();
        auto* torquePidLayout = new QVBoxLayout(torquePidPage);

        struct PidLoop
        {
            const char* name;
            void (CommandPanel::*signal)(float, float, float);
        };

        auto addPidGroup = [this](QVBoxLayout* parentLayout, const char* name, void (CommandPanel::*signal)(float, float, float))
        {
            auto* group = new QGroupBox(name);
            auto* formLayout = new QFormLayout(group);

            auto* kpSpin = new QDoubleSpinBox();
            kpSpin->setRange(0.0, 32.0);
            kpSpin->setDecimals(3);
            auto* kiSpin = new QDoubleSpinBox();
            kiSpin->setRange(0.0, 32.0);
            kiSpin->setDecimals(3);
            auto* kdSpin = new QDoubleSpinBox();
            kdSpin->setRange(0.0, 32.0);
            kdSpin->setDecimals(3);

            formLayout->addRow("Kp:", kpSpin);
            formLayout->addRow("Ki:", kiSpin);
            formLayout->addRow("Kd:", kdSpin);

            auto* sendButton = new QPushButton("Apply");
            formLayout->addRow(sendButton);
            parentLayout->addWidget(group);

            commandButtons.push_back(sendButton);

            connect(sendButton, &QPushButton::clicked, [this, kpSpin, kiSpin, kdSpin, signal]()
                {
                    (this->*signal)(
                        static_cast<float>(kpSpin->value()),
                        static_cast<float>(kiSpin->value()),
                        static_cast<float>(kdSpin->value()));
                });
        };

        addPidGroup(torquePidLayout, "Current PID", &CommandPanel::SetCurrentIdPidRequested);
        torquePidLayout->addStretch();
        pidStack->addWidget(torquePidPage);

        // Index 1: Speed mode — Current Id + Iq + Speed PID
        auto* speedPidPage = new QWidget();
        auto* speedPidLayout = new QVBoxLayout(speedPidPage);
        addPidGroup(speedPidLayout, "Current PID", &CommandPanel::SetCurrentIdPidRequested);
        addPidGroup(speedPidLayout, "Speed PID", &CommandPanel::SetSpeedPidRequested);
        speedPidLayout->addStretch();
        pidStack->addWidget(speedPidPage);

        // Index 2: Position mode — Current Id + Iq + Speed + Position PID
        auto* positionPidPage = new QWidget();
        auto* positionPidLayout = new QVBoxLayout(positionPidPage);
        addPidGroup(positionPidLayout, "Current PID", &CommandPanel::SetCurrentIdPidRequested);
        addPidGroup(positionPidLayout, "Speed PID", &CommandPanel::SetSpeedPidRequested);
        addPidGroup(positionPidLayout, "Position PID", &CommandPanel::SetPositionPidRequested);
        positionPidLayout->addStretch();
        pidStack->addWidget(positionPidPage);

        layout->addWidget(pidStack);
        return page;
    }

    QWidget* CommandPanel::CreateParametersTab()
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);

        auto* systemGroup = new QGroupBox("System Parameters");
        auto* systemLayout = new QFormLayout(systemGroup);

        auto* voltageSpin = new QDoubleSpinBox();
        voltageSpin->setRange(0.0, 327.0);
        voltageSpin->setDecimals(2);
        voltageSpin->setSuffix(" V");
        voltageSpin->setValue(24.0);
        systemLayout->addRow("Supply Voltage:", voltageSpin);
        auto* voltageButton = new QPushButton("Set");
        systemLayout->addRow(voltageButton);

        commandButtons.push_back(voltageButton);
        connect(voltageButton, &QPushButton::clicked, [this, voltageSpin]()
            {
                emit SetSupplyVoltageRequested(static_cast<float>(voltageSpin->value()));
            });

        auto* maxCurrentSpin = new QDoubleSpinBox();
        maxCurrentSpin->setRange(0.0, 32.0);
        maxCurrentSpin->setDecimals(3);
        maxCurrentSpin->setSuffix(" A");
        maxCurrentSpin->setValue(10.0);
        systemLayout->addRow("Max Current:", maxCurrentSpin);
        auto* maxCurrentButton = new QPushButton("Set");
        systemLayout->addRow(maxCurrentButton);

        commandButtons.push_back(maxCurrentButton);
        connect(maxCurrentButton, &QPushButton::clicked, [this, maxCurrentSpin]()
            {
                emit SetMaxCurrentRequested(static_cast<float>(maxCurrentSpin->value()));
            });

        layout->addWidget(systemGroup);

        layout->addStretch();
        return page;
    }

    void CommandPanel::SetCommandsEnabled(bool enabled)
    {
        for (auto* button : commandButtons)
            button->setEnabled(enabled);
    }

    void CommandPanel::SetActiveControlMode(services::FocMotorMode mode)
    {
        int index = static_cast<int>(mode);
        setpointStack->setCurrentIndex(index);
        pidStack->setCurrentIndex(index);

        static const char* modeNames[] = { "Torque", "Speed", "Position" };
        if (index >= 0 && index <= 2)
            modeLabel->setText(QString("Active: %1").arg(modeNames[index]));
    }
}
