#include "tools/can_commander/gui/CommandPanel.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
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
        modeCombo->addItem("Torque", static_cast<int>(can::FocMotorMode::torque));
        modeCombo->addItem("Speed", static_cast<int>(can::FocMotorMode::speed));
        modeCombo->addItem("Position", static_cast<int>(can::FocMotorMode::position));
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
                emit SetControlModeRequested(static_cast<can::FocMotorMode>(modeCombo->currentData().toInt()));
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

        auto addBandwidthGroup = [this](QVBoxLayout* parentLayout, const char* name, float maxBw, void (CommandPanel::*signal)(float))
        {
            auto* group = new QGroupBox(name);
            auto* formLayout = new QFormLayout(group);

            auto* bwSpin = new QDoubleSpinBox();
            bwSpin->setRange(foc::CommandLimits::minBandwidth, static_cast<double>(maxBw));
            bwSpin->setDecimals(1);
            bwSpin->setSuffix(" rad/s");

            formLayout->addRow("Bandwidth:", bwSpin);

            auto* sendButton = new QPushButton("Apply");
            formLayout->addRow(sendButton);
            parentLayout->addWidget(group);

            commandButtons.push_back(sendButton);

            connect(sendButton, &QPushButton::clicked, [this, bwSpin, signal]()
                {
                    (this->*signal)(static_cast<float>(bwSpin->value()));
                });
        };

        // Index 0: Torque mode — Current bandwidth
        auto* torquePidPage = new QWidget();
        auto* torquePidLayout = new QVBoxLayout(torquePidPage);
        addBandwidthGroup(torquePidLayout, "Current Loop Bandwidth", foc::CommandLimits::maxCurrentBandwidth, &CommandPanel::SetCurrentBandwidthRequested);
        torquePidLayout->addStretch();
        pidStack->addWidget(torquePidPage);

        // Index 1: Speed mode — Current + Speed bandwidth
        auto* speedPidPage = new QWidget();
        auto* speedPidLayout = new QVBoxLayout(speedPidPage);
        addBandwidthGroup(speedPidLayout, "Current Loop Bandwidth", foc::CommandLimits::maxCurrentBandwidth, &CommandPanel::SetCurrentBandwidthRequested);
        addBandwidthGroup(speedPidLayout, "Speed Loop Bandwidth", foc::CommandLimits::maxSpeedBandwidth, &CommandPanel::SetSpeedBandwidthRequested);
        speedPidLayout->addStretch();
        pidStack->addWidget(speedPidPage);

        // Index 2: Position mode — Current + Speed + Position bandwidth
        auto* positionPidPage = new QWidget();
        auto* positionPidLayout = new QVBoxLayout(positionPidPage);
        addBandwidthGroup(positionPidLayout, "Current Loop Bandwidth", foc::CommandLimits::maxCurrentBandwidth, &CommandPanel::SetCurrentBandwidthRequested);
        addBandwidthGroup(positionPidLayout, "Speed Loop Bandwidth", foc::CommandLimits::maxSpeedBandwidth, &CommandPanel::SetSpeedBandwidthRequested);
        addBandwidthGroup(positionPidLayout, "Position Loop Bandwidth", foc::CommandLimits::maxPositionBandwidth, &CommandPanel::SetPositionBandwidthRequested);
        positionPidLayout->addStretch();
        pidStack->addWidget(positionPidPage);

        layout->addWidget(pidStack);
        return page;
    }

    QWidget* CommandPanel::CreateParametersTab()
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->addStretch();
        return page;
    }

    void CommandPanel::SetCommandsEnabled(bool enabled)
    {
        for (auto* button : commandButtons)
            button->setEnabled(enabled);
    }

    void CommandPanel::SetActiveControlMode(can::FocMotorMode mode)
    {
        int index = static_cast<int>(mode);
        setpointStack->setCurrentIndex(index);
        pidStack->setCurrentIndex(index);

        static const char* modeNames[] = { "Torque", "Speed", "Position" };
        if (index >= 0 && index <= 2)
            modeLabel->setText(QString("Active: %1").arg(modeNames[index]));
    }
}
