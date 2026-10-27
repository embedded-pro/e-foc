#include "tools/can_commander/gui/CommandPanel.hpp"
#include "tools/can_commander/logic/SetpointAxis.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QSignalBlocker>

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
        diagnostics = new DiagnosticsTab();

        tabs = new QTabWidget();
        tabs->addTab(CreateMotorControlTab(), "&Motor Control");
        tabs->addTab(controllers, "C&ontrollers");
        tabs->addTab(motorParameters, "Motor &Parameters");
        tabs->addTab(configuration, "Confi&guration");
        tabs->addTab(diagnostics, "Diagnostics");
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

    DiagnosticsTab& CommandPanel::Diagnostics() const
    {
        return *diagnostics;
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
        setpoints[0].mode = can::FocMotorMode::torque;
        setpoints[1].mode = can::FocMotorMode::speed;
        setpoints[2].mode = can::FocMotorMode::position;

        setpointStack = new QStackedWidget();
        setpointStack->addWidget(CreateSetpointGroup(setpoints[0], "Torque Setpoint", "Iq:"));
        setpointStack->addWidget(CreateSetpointGroup(setpoints[1], "Speed Setpoint", "Speed:"));
        setpointStack->addWidget(CreateSetpointGroup(setpoints[2], "Position Setpoint", "Position:"));
        return setpointStack;
    }

    QGroupBox* CommandPanel::CreateSetpointGroup(SetpointControls& controls, const char* title, const char* label)
    {
        const auto& axis = AxisFor(controls.mode);
        auto* group = new QGroupBox(title);
        auto* layout = new QHBoxLayout(group);

        controls.knob = new SetpointKnob(axis);
        const auto range = QString("Accepted by the firmware: ±%1").arg(controls.knob->Format(axis.MaxCounts()));
        controls.knob->setAccessibleName(QString("%1 knob").arg(title));
        controls.knob->setToolTip(QString("Turn to choose the setpoint, then Apply. Arrow keys step, Page Up and Page Down jump.\n%1").arg(range));
        layout->addWidget(controls.knob);
        layout->addLayout(CreateSetpointEntry(controls, title, label, range), 1);

        commandWidgets.push_back(controls.apply);
        ConnectSetpoint(controls);
        RefreshSetpointStatus(controls);
        return group;
    }

    QLayout* CommandPanel::CreateSetpointEntry(SetpointControls& controls, const char* title, const char* label, const QString& range)
    {
        const auto& axis = AxisFor(controls.mode);
        controls.spin = new QDoubleSpinBox();
        controls.spin->setDecimals(axis.Decimals());
        controls.spin->setRange(axis.DisplayOf(-axis.MaxCounts()), axis.DisplayOf(axis.MaxCounts()));
        controls.spin->setSingleStep(axis.DisplayStep());
        controls.spin->setSuffix(ui::backend::qt::ToQt(axis.Suffix()));
        controls.spin->setKeyboardTracking(false);
        controls.spin->setMinimumWidth(140);
        controls.spin->setAccessibleName(title);
        controls.spin->setToolTip(QString("%1\nEnter applies the value.").arg(range));
        auto* form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
        form->addRow(label, controls.spin);

        controls.apply = new QPushButton("Apply");
        auto* buttons = new QHBoxLayout();
        buttons->addWidget(controls.apply);
        buttons->addStretch();

        controls.status = new QLabel();
        controls.status->setWordWrap(true);

        auto* column = new QVBoxLayout();
        column->addStretch();
        column->addLayout(form);
        column->addLayout(buttons);
        column->addWidget(controls.status);
        column->addStretch();
        return column;
    }

    void CommandPanel::ConnectSetpoint(SetpointControls& controls)
    {
        controls.knob->installEventFilter(this);
        controls.spin->installEventFilter(this);

        connect(controls.knob, &QDial::valueChanged, this, [this, &controls](int counts)
            {
                SyncSetpoint(controls, counts);
            });
        connect(controls.spin, &QDoubleSpinBox::valueChanged, this, [this, &controls](double value)
            {
                SyncSetpoint(controls, AxisFor(controls.mode).CountsOf(value));
            });
        connect(controls.apply, &QPushButton::clicked, this, [this, &controls]()
            {
                emit SetpointRequested(controls.mode, controls.knob->value());
            });
    }

    void CommandPanel::SyncSetpoint(SetpointControls& controls, int32_t counts)
    {
        {
            const QSignalBlocker knobBlocker{ controls.knob };
            const QSignalBlocker spinBlocker{ controls.spin };
            controls.knob->setValue(counts);
            controls.spin->setValue(AxisFor(controls.mode).DisplayOf(counts));
        }
        RefreshSetpointStatus(controls);
    }

    void CommandPanel::RefreshSetpointStatus(SetpointControls& controls)
    {
        controls.knob->SetAccepted(controls.accepted);

        const auto chosen = controls.knob->value();
        auto text = QString("Last accepted: %1").arg(controls.accepted.has_value() ? controls.knob->Format(*controls.accepted) : QString("—"));
        auto level = ui::theme::StatusLevel::Neutral;
        if (controls.accepted != chosen)
        {
            text += QString(". Apply sends %1.").arg(controls.knob->Format(chosen));
            if (controls.accepted.has_value())
                level = ui::theme::StatusLevel::Warning;
        }

        controls.status->setText(text);
        ui::backend::qt::StyleStatusLabel(*controls.status, level);
    }

    bool CommandPanel::eventFilter(QObject* watched, QEvent* event)
    {
        if (event->type() != QEvent::KeyPress)
            return QWidget::eventFilter(watched, event);

        const auto key = static_cast<QKeyEvent*>(event)->key();
        if (key != Qt::Key_Return && key != Qt::Key_Enter)
            return QWidget::eventFilter(watched, event);

        for (auto& controls : setpoints)
            if (watched == controls.knob || watched == controls.spin)
            {
                controls.spin->interpretText();
                if (controls.apply->isEnabled())
                    controls.apply->click();
                return true;
            }

        return QWidget::eventFilter(watched, event);
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

        activeMode = mode;
        setpointStack->setCurrentIndex(index);
        modeLabel->setText(QString("Active: %1").arg(ControlModeName(mode)));
        ui::backend::qt::StyleStatusLabel(*modeLabel, ui::theme::StatusLevel::Ok);
        controllers->SetMode(mode);
    }

    void CommandPanel::ClearActiveControlMode()
    {
        activeMode.reset();
        modeLabel->setText("Active: unknown");
        ui::backend::qt::StyleStatusLabel(*modeLabel, ui::theme::StatusLevel::Neutral);
        controllers->SetMode(std::nullopt);
    }

    std::optional<can::FocMotorMode> CommandPanel::ActiveControlMode() const
    {
        return activeMode;
    }

    void CommandPanel::ShowAcceptedSetpoint(can::FocMotorMode mode, int32_t counts)
    {
        for (auto& controls : setpoints)
            if (controls.mode == mode)
            {
                controls.accepted = counts;
                RefreshSetpointStatus(controls);
            }
    }

    void CommandPanel::ForgetAcceptedSetpoints()
    {
        for (auto& controls : setpoints)
        {
            controls.accepted.reset();
            RefreshSetpointStatus(controls);
        }
    }

    void CommandPanel::ShowMotorParameters()
    {
        tabs->setCurrentWidget(motorParameters);
    }
}
