#include "tools/virtual_motor/app/gui/ProfilePanel.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace virtual_motor::app
{
    namespace
    {
        constexpr uint32_t defaultControlRateHz{ 20000 };
        constexpr const char* fileFilter{ "Virtual motor profiles (*.profile);;Text files (*.txt);;All files (*)" };
        constexpr const char* example{
            "# One change per line, timed in seconds after the trigger.\n"
            "trigger immediately\n"
            "at 0 load 0.02\n"
            "at 0.5 external sine 0 0.01 5\n"
            "at 2 external none\n"
        };
    }

    ProfilePanel::ProfilePanel(RigMotor& motor, QWidget* parent)
        : QGroupBox("Profile", parent)
        , motor(motor)
        , editor(new QPlainTextEdit(this))
        , status(new QLabel(this))
    {
        editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        editor->setPlainText(example);
        editor->setAccessibleName("Profile text");
        editor->setToolTip("trigger and at lines, as in the design document; Start reads the text as it stands");
        editor->setMinimumHeight(120);
        editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        status->setWordWrap(true);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(editor);
        layout->addWidget(BuildButtons());
        layout->addWidget(status);
        Refresh();
    }

    QWidget* ProfilePanel::BuildButtons()
    {
        auto* buttons = new QWidget(this);
        auto* layout = new QHBoxLayout(buttons);
        layout->setContentsMargins(0, 0, 0, 0);
        load = new QPushButton("Load…", buttons);
        save = new QPushButton("Save…", buttons);
        start = new QPushButton("Start", buttons);
        stop = new QPushButton("Stop", buttons);
        ui::backend::qt::StyleButton(*start, ui::theme::ButtonRole::Primary);
        stop->setToolTip("Stop the profile; the plant returns to its description");
        for (auto* button : { load, save, start, stop })
            layout->addWidget(button);
        connect(load, &QPushButton::clicked, this, &ProfilePanel::Load);
        connect(save, &QPushButton::clicked, this, &ProfilePanel::Save);
        connect(start, &QPushButton::clicked, this, &ProfilePanel::Start);
        connect(stop, &QPushButton::clicked, this, [this]()
            {
                this->motor.Send(port::StopProfile{});
            });
        return buttons;
    }

    void ProfilePanel::Refresh()
    {
        const bool connected = motor.State() == MotorState::running || motor.State() == MotorState::paused;
        const auto& state = motor.Profile();
        const bool active = state.has_value() && (state->state == port::ProfileStateKind::armed || state->state == port::ProfileStateKind::running);
        start->setEnabled(connected);
        start->setToolTip(connected ? "Read the profile and send it to the motor; it replaces a running one" : "Available once the motor has introduced itself on the plant port");
        stop->setEnabled(connected && active);

        const auto text = problem.isEmpty() ? QString::fromStdString(ProfileStatusText(state, started.has_value() ? &*started : nullptr, ControlRate())) : problem;
        status->setText(text);
        auto level = active ? ui::theme::StatusLevel::Ok : ui::theme::StatusLevel::Neutral;
        if (!problem.isEmpty())
            level = ui::theme::StatusLevel::Warning;
        ui::backend::qt::StyleStatusLabel(*status, level);
    }

    void ProfilePanel::ShowRefusal(const port::Refused& refused)
    {
        problem = refused.reason == port::RefusalReason::outOfRange ? "Not started: a change in the profile is out of range, so the motor refused all of it." : "Not started: the motor does not know a change in the profile.";
        Refresh();
    }

    void ProfilePanel::SetText(const QString& text)
    {
        editor->setPlainText(text);
    }

    void ProfilePanel::Start()
    {
        const auto parsed = ParseProfile(editor->toPlainText().toStdString());
        if (!parsed.profile.has_value())
        {
            problem = "Not started: " + QString::fromStdString(parsed.error);
            Refresh();
            return;
        }
        problem.clear();
        started = parsed.profile;
        motor.Send(ProfileRequestFor(*started, ControlRate()));
        Refresh();
    }

    void ProfilePanel::Load()
    {
        const auto path = QFileDialog::getOpenFileName(this, "Load Profile", {}, fileFilter);
        if (path.isEmpty())
            return;
        QFile file{ path };
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "Load Profile", "The profile could not be read from " + path + ": " + file.errorString());
            return;
        }
        editor->setPlainText(QString::fromUtf8(file.readAll()));
        setTitle("Profile: " + QFileInfo(path).fileName());
    }

    void ProfilePanel::Save()
    {
        const auto path = QFileDialog::getSaveFileName(this, "Save Profile", "disturbance.profile", fileFilter);
        if (path.isEmpty())
            return;
        QFile file{ path };
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) || file.write(editor->toPlainText().toUtf8()) < 0)
        {
            QMessageBox::warning(this, "Save Profile", "The profile could not be written to " + path + ": " + file.errorString());
            return;
        }
        setTitle("Profile: " + QFileInfo(path).fileName());
    }

    uint32_t ProfilePanel::ControlRate() const
    {
        return motor.Identity().has_value() ? motor.Identity()->controlRateHz : defaultControlRateHz;
    }
}
