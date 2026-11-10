#include "tools/virtual_motor/app/gui/MotorWindow.hpp"
#include "tools/virtual_motor/app/gui/LogView.hpp"
#include "tools/virtual_motor/app/gui/TorqueNoisePanel.hpp"
#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include "tools/virtual_motor/platform/PlantDescription.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

namespace virtual_motor::app
{
    namespace
    {
        QString Text(const std::string& text)
        {
            return QString::fromStdString(text);
        }

        ui::theme::StatusLevel LevelOf(MotorState state)
        {
            switch (state)
            {
                case MotorState::running:
                    return ui::theme::StatusLevel::Ok;
                case MotorState::paused:
                case MotorState::starting:
                case MotorState::restarting:
                    return ui::theme::StatusLevel::Warning;
                case MotorState::crashed:
                    return ui::theme::StatusLevel::Fault;
                default:
                    return ui::theme::StatusLevel::Neutral;
            }
        }
    }

    MotorWindow::MotorWindow(Rig& rig, uint8_t node, QWidget* parent)
        : QWidget(parent, Qt::Window)
        , rig(rig)
        , node(node)
    {
        auto& motor = *rig.Find(node);
        setWindowTitle(QString("VIRTUAL · Node %1 · %2 — e-foc Virtual Motor").arg(node).arg(Text(motor.Spec().plant)));

        panel = new TorqueNoisePanel(motor, PlantDescription(motor.Spec().plant), this);
        log = new LogView(this);
        QStringList lines;
        for (const auto& line : motor.Log())
            lines << Text(line);
        log->SetLines(lines);

        auto* views = new QTabWidget(this);
        views->addTab(log, "Log");
        auto* side = new QScrollArea(this);
        side->setWidget(panel);
        side->setWidgetResizable(true);
        auto* splitter = new QSplitter(this);
        splitter->addWidget(views);
        splitter->addWidget(side);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(BuildHeader());
        layout->addWidget(BuildControls());
        layout->addWidget(splitter, 1);
        resize(1100, 700);
        Refresh();
    }

    QWidget* MotorWindow::BuildHeader()
    {
        auto* header = new QWidget(this);
        auto* layout = new QHBoxLayout(header);
        auto* badge = new QLabel("VIRTUAL", header);
        badge->setToolTip("A virtual motor: the e-foc application running against a simulated plant, not hardware");
        ui::backend::qt::StyleStatusLabel(*badge, ui::theme::StatusLevel::Warning);
        auto* title = new QLabel(QString("Node %1 · %2").arg(node).arg(Text(rig.Find(node)->Spec().plant)), header);
        state = new QLabel(header);
        time = new QLabel(header);
        realTime = new QLabel(header);
        leds = new QLabel(header);
        protection = new QLabel(header);
        for (auto* widget : { static_cast<QWidget*>(badge), static_cast<QWidget*>(title), static_cast<QWidget*>(state), static_cast<QWidget*>(time),
                 static_cast<QWidget*>(realTime), static_cast<QWidget*>(leds), static_cast<QWidget*>(protection) })
            layout->addWidget(widget);
        layout->addStretch(1);
        return header;
    }

    QWidget* MotorWindow::BuildControls()
    {
        auto* controls = new QWidget(this);
        auto* layout = new QHBoxLayout(controls);
        pause = new QPushButton("Pause", controls);
        step = new QPushButton("Step 1 ms", controls);
        powerOn = new QPushButton("Power On", controls);
        powerCycle = new QPushButton("Power Cycle", controls);
        powerOff = new QPushButton("Power Off", controls);
        ui::backend::qt::StyleButton(*powerOff, ui::theme::ButtonRole::Stop);
        disturbances = new QLabel(controls);
        clearAll = new QPushButton("Clear All", controls);
        clearAll->setToolTip("Return the plant to its description: remove every torque and noise change");
        for (auto* button : { pause, step, powerOn, powerCycle, powerOff })
            layout->addWidget(button);
        layout->addStretch(1);
        layout->addWidget(disturbances);
        layout->addWidget(clearAll);

        ConnectControls();
        return controls;
    }

    void MotorWindow::ConnectControls()
    {
        connect(pause, &QPushButton::clicked, this, [this]()
            {
                auto* motor = rig.Find(node);
                if (motor->State() == MotorState::paused)
                    motor->Send(port::Resume{});
                else
                    motor->Send(port::Pause{});
            });
        connect(step, &QPushButton::clicked, this, [this]()
            {
                rig.Find(node)->Send(port::Step{ port::StepUnit::millisecond, 1 });
            });
        connect(powerOn, &QPushButton::clicked, this, [this]()
            {
                rig.PowerOn(node);
            });
        connect(powerCycle, &QPushButton::clicked, this, [this]()
            {
                rig.PowerCycle(node);
            });
        connect(powerOff, &QPushButton::clicked, this, [this]()
            {
                rig.PowerOff(node);
            });
        connect(clearAll, &QPushButton::clicked, this, [this]()
            {
                rig.Find(node)->Send(port::ClearAll{});
            });
    }

    void MotorWindow::Refresh()
    {
        const auto* motor = rig.Find(node);
        if (motor == nullptr)
            return;

        state->setText(Text(StateText(motor->State())));
        ui::backend::qt::StyleStatusLabel(*state, LevelOf(motor->State()));
        RefreshStatus(*motor);
        RefreshControls(*motor);
        panel->Refresh();
    }

    void MotorWindow::RefreshStatus(const RigMotor& motor)
    {
        const auto& status = motor.Status();
        const auto rate = motor.Identity() ? motor.Identity()->controlRateHz : 0u;
        time->setText("t = " + (status ? Text(VirtualTimeText(status->controlPeriods, rate)) : QString("—")));
        if (!status)
        {
            realTime->setText("RT —");
            leds->setText("LEDs —");
            protection->setText("Protection —");
            return;
        }

        const auto pacing = RealTimeText(*status);
        realTime->setText("RT " + Text(pacing.text));
        realTime->setToolTip(Text(pacing.detail));
        ui::backend::qt::StyleStatusLabel(*realTime, pacing.late ? ui::theme::StatusLevel::Warning : ui::theme::StatusLevel::Neutral);
        leds->setText("LEDs: " + Text(LedText(status->leds)));
        protection->setText("Protection: " + Text(ProtectionText(status->protection)));
        ui::backend::qt::StyleStatusLabel(*protection, status->protection == 1 ? ui::theme::StatusLevel::Fault : ui::theme::StatusLevel::Neutral);
    }

    void MotorWindow::RefreshControls(const RigMotor& motor)
    {
        const bool connected = motor.State() == MotorState::running || motor.State() == MotorState::paused;
        const bool running = motor.State() != MotorState::stopped && motor.State() != MotorState::crashed;
        const QString notConnected{ "Available once the motor has introduced itself on the plant port" };

        pause->setText(motor.State() == MotorState::paused ? "Resume" : "Pause");
        pause->setEnabled(connected);
        pause->setToolTip(connected ? "Pausing stops virtual time; the CAN Commander sees the motor go offline" : notConnected);
        step->setEnabled(motor.State() == MotorState::paused);
        step->setToolTip(motor.State() == MotorState::paused ? "Advance virtual time by one millisecond" : "Pause the motor to step it");
        clearAll->setEnabled(connected && motor.ActiveDisturbances() != 0);
        powerOn->setEnabled(!running);
        powerCycle->setEnabled(true);
        powerOff->setEnabled(running);
        disturbances->setText(QString("Active disturbances: %1").arg(motor.ActiveDisturbances()));
        panel->setEnabled(connected);
        panel->setToolTip(connected ? QString{} : notConnected);
    }

    void MotorWindow::AppendLog(const QString& line)
    {
        log->Append(line);
    }

    void MotorWindow::ShowRefusal(const port::Refused& refused)
    {
        panel->ShowRefusal(refused);
    }
}
