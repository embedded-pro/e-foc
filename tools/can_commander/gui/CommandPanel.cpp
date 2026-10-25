#include "tools/can_commander/gui/CommandPanel.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "tools/can_commander/gui/DisplayFormat.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QFormLayout>
#include <QHBoxLayout>

namespace tool
{
    QString CommandPanel::ControlModeName(can::FocMotorMode mode)
    {
        switch (mode)
        {
            case can::FocMotorMode::torque:
                return "Torque";
            case can::FocMotorMode::speed:
                return "Speed";
            case can::FocMotorMode::position:
                return "Position";
            default:
                return "Unknown";
        }
    }

    CommandPanel::CommandPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        hintLabel = new QLabel();
        hintLabel->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*hintLabel, ui::theme::StatusLevel::Warning);
        layout->addWidget(hintLabel);

        controllers = new ControllersTab();
        motorParameters = new MotorParametersTab();
        configuration = new ConfigurationTab();

        tabs = new QTabWidget();
        tabs->addTab(CreateMotorControlTab(), "&Motor Control");
        tabs->addTab(controllers, "C&ontrollers");
        tabs->addTab(motorParameters, "Motor &Parameters");
        tabs->addTab(configuration, "Confi&guration");
        layout->addWidget(tabs, 1);
    }

    ControllersTab& CommandPanel::Controllers() const
    {
        return *controllers;
    }

    MotorParametersTab& CommandPanel::MotorParameters() const
    {
        return *motorParameters;
    }

    ConfigurationTab& CommandPanel::Configuration() const
    {
        return *configuration;
    }

    std::array<SettingsPage*, 3> CommandPanel::SettingsPages() const
    {
        return { controllers, motorParameters, configuration };
    }

    QWidget* CommandPanel::CreateMotorControlTab()
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(CreateControlGroup());
        layout->addWidget(CreateModeGroup());
        layout->addWidget(CreateSetpointStack());

        auto* queryButton = new QPushButton("Chec&k Firmware Compatibility");
        queryButton->setToolTip("Ask the controller for its CAN contract version");
        layout->addWidget(queryButton);
        commandWidgets.push_back(queryButton);
        connect(queryButton, &QPushButton::clicked, this, &CommandPanel::RequestDataRequested);

        layout->addStretch();
        return page;
    }

    QGroupBox* CommandPanel::CreateControlGroup()
    {
        auto* group = new QGroupBox("Motor");
        auto* layout = new QHBoxLayout(group);

        auto* startButton = new QPushButton("&Start Motor");
        auto* stopButton = new QPushButton("S&top Motor");
        auto* clearFaultButton = new QPushButton("Clear F&ault");
        layout->addWidget(startButton);
        layout->addWidget(stopButton);
        layout->addWidget(clearFaultButton);

        commandWidgets.push_back(startButton);
        commandWidgets.push_back(stopButton);
        commandWidgets.push_back(clearFaultButton);
        connect(startButton, &QPushButton::clicked, this, &CommandPanel::StartMotorRequested);
        connect(stopButton, &QPushButton::clicked, this, &CommandPanel::StopMotorRequested);
        connect(clearFaultButton, &QPushButton::clicked, this, &CommandPanel::ClearFaultRequested);
        return group;
    }

    QGroupBox* CommandPanel::CreateModeGroup()
    {
        auto* group = new QGroupBox("Control Mode");
        auto* layout = new QHBoxLayout(group);

        auto* modeCombo = new QComboBox();
        modeCombo->setAccessibleName("Control mode");
        for (const auto mode : { can::FocMotorMode::torque, can::FocMotorMode::speed, can::FocMotorMode::position })
            modeCombo->addItem(ControlModeName(mode), static_cast<int>(mode));
        auto* modeButton = new QPushButton("Apply");
        modeLabel = new QLabel();
        layout->addWidget(modeCombo);
        layout->addWidget(modeButton);
        layout->addWidget(modeLabel, 1);
        ClearActiveControlMode();

        commandWidgets.push_back(modeButton);
        connect(modeButton, &QPushButton::clicked, this, [this, modeCombo]()
            {
                emit SetControlModeRequested(static_cast<can::FocMotorMode>(modeCombo->currentData().toInt()));
            });
        return group;
    }

    QStackedWidget* CommandPanel::CreateSetpointStack()
    {
        using foc::CommandLimits;

        setpointStack = new QStackedWidget();
        setpointStack->addWidget(CreateSetpointGroup("Torque Setpoint", "Iq:", SetpointLimit(CommandLimits::maxTorqueSetpoint, can::focCurrentScale),
            DecimalsFor(can::focCurrentScale), " A", &CommandPanel::SetTorqueSetpointRequested));
        setpointStack->addWidget(CreateSetpointGroup("Speed Setpoint", "Speed:", SetpointLimit(CommandLimits::maxSpeedSetpoint, can::focSpeedScale),
            DecimalsFor(can::focSpeedScale), " rad/s", &CommandPanel::SetSpeedSetpointRequested));
        setpointStack->addWidget(CreateSetpointGroup("Position Setpoint", "Position:", SetpointLimit(CommandLimits::maxPositionSetpoint, can::focPositionScale),
            DecimalsFor(can::focPositionScale), " rad", &CommandPanel::SetPositionSetpointRequested));
        return setpointStack;
    }

    QGroupBox* CommandPanel::CreateSetpointGroup(const char* title, const char* label, double limit, int decimals, const char* suffix, void (CommandPanel::*signal)(float))
    {
        auto* group = new QGroupBox(title);
        auto* layout = new QFormLayout(group);

        auto* spin = new QDoubleSpinBox();
        spin->setRange(-limit, limit);
        spin->setDecimals(decimals);
        spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);
        spin->setToolTip(QString("Accepted by the firmware: ±%1%2").arg(limit, 0, 'f', decimals).arg(suffix));
        layout->addRow(label, spin);

        auto* applyButton = new QPushButton("Apply");
        layout->addRow(applyButton);

        commandWidgets.push_back(applyButton);
        connect(applyButton, &QPushButton::clicked, this, [this, spin, signal]()
            {
                emit(this->*signal)(static_cast<float>(spin->value()));
            });
        return group;
    }

    void CommandPanel::SetCommandsAvailable(const QString& unavailableReason, bool explained)
    {
        const bool available = unavailableReason.isEmpty();
        for (auto* widget : commandWidgets)
        {
            widget->setEnabled(available);
            widget->setToolTip(available ? QString{} : unavailableReason);
        }

        hintLabel->setText(unavailableReason);
        hintLabel->setVisible(!available && explained);

        motorParameters->SetActionsAvailable(unavailableReason);
        configuration->SetUnavailable(unavailableReason);
    }

    void CommandPanel::SetSettingsAvailable(const QString& unavailableReason)
    {
        controllers->SetUnavailable(unavailableReason);
        motorParameters->SetUnavailable(unavailableReason);
    }

    void CommandPanel::ShowRequestedControlMode(can::FocMotorMode mode)
    {
        modeLabel->setText(QString("Requested: %1…").arg(ControlModeName(mode)));
        ui::backend::qt::StyleStatusLabel(*modeLabel, ui::theme::StatusLevel::Warning);
    }

    void CommandPanel::SetActiveControlMode(can::FocMotorMode mode)
    {
        const int index = static_cast<int>(mode);
        if (index < 0 || index >= setpointStack->count())
            return;

        setpointStack->setCurrentIndex(index);
        modeLabel->setText(QString("Active: %1").arg(ControlModeName(mode)));
        ui::backend::qt::StyleStatusLabel(*modeLabel, ui::theme::StatusLevel::Ok);
        controllers->SetMode(mode);
    }

    void CommandPanel::ClearActiveControlMode()
    {
        modeLabel->setText("Active: unknown");
        ui::backend::qt::StyleStatusLabel(*modeLabel, ui::theme::StatusLevel::Neutral);
        controllers->SetMode(std::nullopt);
    }
}
