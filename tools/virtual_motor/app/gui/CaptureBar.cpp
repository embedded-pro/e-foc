#include "tools/virtual_motor/app/gui/CaptureBar.hpp"
#include "tools/virtual_motor/app/logic/TraceSignals.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

namespace virtual_motor::app
{
    namespace
    {
        struct KindChoice
        {
            const char* text;
            port::TriggerKind kind;
        };

        constexpr std::array<KindChoice, 6> kinds{ {
            { "Immediately", port::TriggerKind::immediately },
            { "Rising edge", port::TriggerKind::rising },
            { "Falling edge", port::TriggerKind::falling },
            { "Either edge", port::TriggerKind::either },
            { "Board protection", port::TriggerKind::protection },
            { "LEDs", port::TriggerKind::leds },
        } };

        QString Text(std::string_view text)
        {
            return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
        }

        constexpr std::array<const char*, 3> ledNames{ "Operational", "Warning", "Failure" };
        constexpr std::array<uint8_t, 3> ledBits{ port::led::operational, port::led::warning, port::led::failure };
    }

    CaptureBar::CaptureBar(RigMotor& motor, QWidget* parent)
        : QWidget(parent)
        , motor(motor)
    {
        BuildFields();
        ConnectFields();
        KindChanged();
        SignalChanged();
        Refresh();
    }

    void CaptureBar::BuildFields()
    {
        auto* layout = new QHBoxLayout(this);
        layout->addWidget(new QLabel("Capture: trigger", this));
        kind = new QComboBox(this);
        kind->setAccessibleName("Capture trigger");
        for (const auto& choice : kinds)
            kind->addItem(choice.text);
        signal = new QComboBox(this);
        signal->setAccessibleName("Trigger signal");
        for (const auto& info : Signals())
            signal->addItem(Text(info.name), Text(info.description));
        level = new QDoubleSpinBox(this);
        level->setAccessibleName("Trigger level");
        level->setRange(-1e6, 1e6);
        level->setDecimals(4);
        level->setKeyboardTracking(false);
        for (std::size_t index = 0; index != leds.size(); ++index)
            leds[index] = new QCheckBox(ledNames[index], this);
        preTrigger = new QSpinBox(this);
        preTrigger->setAccessibleName("Share of the capture before the trigger");
        preTrigger->setRange(0, 100);
        preTrigger->setValue(25);
        preTrigger->setPrefix("pre ");
        preTrigger->setSuffix(" %");
        length = new QSpinBox(this);
        length->setAccessibleName("Capture length");
        length->setRange(16, port::maxCapturePeriods);
        length->setValue(4096);
        length->setSuffix(" periods");
        arm = new QPushButton("Arm", this);
        ui::backend::qt::StyleButton(*arm, ui::theme::ButtonRole::Primary);
        cancel = new QPushButton("Cancel", this);
        status = new QLabel(this);
        for (auto* widget : std::initializer_list<QWidget*>{ kind, signal, level, leds[0], leds[1], leds[2], preTrigger, length, arm, cancel })
            layout->addWidget(widget);
        layout->addStretch(1);
        layout->addWidget(status);
    }

    void CaptureBar::ConnectFields()
    {
        connect(kind, &QComboBox::currentIndexChanged, this, &CaptureBar::KindChanged);
        connect(signal, &QComboBox::currentIndexChanged, this, &CaptureBar::SignalChanged);
        for (auto* led : leds)
            connect(led, &QCheckBox::toggled, this, &CaptureBar::Refresh);
        connect(arm, &QPushButton::clicked, this, &CaptureBar::Arm);
        connect(cancel, &QPushButton::clicked, this, [this]()
            {
                motor.Send(port::CancelCapture{});
            });
    }

    void CaptureBar::KindChanged()
    {
        signal->setVisible(UsesSignal());
        level->setVisible(UsesSignal());
        for (auto* led : leds)
            led->setVisible(UsesLeds());
        Refresh();
    }

    void CaptureBar::SignalChanged()
    {
        const auto& info = Signals()[static_cast<std::size_t>(signal->currentIndex())];
        level->setSuffix(info.unit == "1" ? QString{} : " " + Text(info.unit));
        signal->setToolTip(signal->currentData().toString());
    }

    void CaptureBar::Arm()
    {
        armedSignal = UsesSignal() ? std::optional<uint8_t>{ static_cast<uint8_t>(signal->currentIndex()) } : std::nullopt;
        motor.Send(Request());
    }

    void CaptureBar::Refresh()
    {
        const bool connected = motor.State() == MotorState::running || motor.State() == MotorState::paused;
        const bool ledsChosen = !UsesLeds() || LedMask() != 0;
        arm->setEnabled(connected && ledsChosen);
        if (!connected)
            arm->setToolTip("Available once the motor has introduced itself on the plant port");
        else if (!ledsChosen)
            arm->setToolTip("Choose at least one LED to trigger on");
        else
            arm->setToolTip("Arm a capture; arming again replaces one armed or recording");
        cancel->setEnabled(connected && motor.Captures().InFlight());
        cancel->setToolTip(motor.Captures().InFlight() ? "Stop the armed or recording capture" : "No capture is armed or recording");
        status->setText(QString::fromStdString(motor.Captures().StatusText()));
        ui::backend::qt::StyleStatusLabel(*status, motor.Captures().InFlight() ? ui::theme::StatusLevel::Warning : ui::theme::StatusLevel::Neutral);
    }

    port::CaptureRequest CaptureBar::Request() const
    {
        port::Trigger trigger{ kinds[static_cast<std::size_t>(kind->currentIndex())].kind, static_cast<uint8_t>(signal->currentIndex()), static_cast<float>(level->value()), LedMask() };
        return { trigger, static_cast<uint8_t>(preTrigger->value()), static_cast<uint16_t>(length->value()) };
    }

    std::optional<uint8_t> CaptureBar::ArmedSignal() const
    {
        return armedSignal;
    }

    bool CaptureBar::UsesSignal() const
    {
        const auto chosen = kinds[static_cast<std::size_t>(kind->currentIndex())].kind;
        return chosen == port::TriggerKind::rising || chosen == port::TriggerKind::falling || chosen == port::TriggerKind::either;
    }

    bool CaptureBar::UsesLeds() const
    {
        return kinds[static_cast<std::size_t>(kind->currentIndex())].kind == port::TriggerKind::leds;
    }

    uint8_t CaptureBar::LedMask() const
    {
        uint8_t mask{ 0 };
        for (std::size_t index = 0; index != leds.size(); ++index)
            if (leds[index]->isChecked())
                mask = static_cast<uint8_t>(mask + ledBits[index]);
        return mask;
    }
}
