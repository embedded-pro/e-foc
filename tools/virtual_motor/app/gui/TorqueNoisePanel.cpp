#include "tools/virtual_motor/app/gui/TorqueNoisePanel.hpp"
#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace virtual_motor::app
{
    namespace
    {
        class EnterApplies
            : public QObject
        {
        public:
            EnterApplies(QPushButton& apply, QObject* parent)
                : QObject(parent)
                , apply(apply)
            {}

            bool eventFilter(QObject* watched, QEvent* event) override
            {
                if (event->type() == QEvent::KeyPress)
                {
                    const auto key = static_cast<QKeyEvent*>(event)->key();
                    if (key == Qt::Key_Return || key == Qt::Key_Enter)
                    {
                        apply.click();
                        return true;
                    }
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            QPushButton& apply;
        };

        struct FormFields
        {
            const char* first;
            const char* second;
            const char* third;
        };

        FormFields FieldsOf(int form)
        {
            switch (static_cast<port::TorqueForm>(form))
            {
                case port::TorqueForm::constant:
                    return { "Torque", nullptr, nullptr };
                case port::TorqueForm::step:
                    return { "Initial", "Final", "Delay" };
                case port::TorqueForm::ramp:
                    return { "Start", "End", "Duration" };
                case port::TorqueForm::sine:
                    return { "Offset", "Amplitude", "Frequency" };
                default:
                    return { nullptr, nullptr, nullptr };
            }
        }

        bool Same(const port::Changes& a, const port::Changes& b)
        {
            return a.loadTorque == b.loadTorque && a.externalTorque.form == b.externalTorque.form && a.externalTorque.parameters == b.externalTorque.parameters &&
                   a.currentNoise == b.currentNoise && a.encoderNoise == b.encoderNoise && a.differs == b.differs;
        }

        QString Refused(uint8_t request)
        {
            switch (static_cast<port::MessageType>(request))
            {
                case port::MessageType::loadTorque:
                    return "load torque";
                case port::MessageType::externalTorque:
                    return "external torque";
                case port::MessageType::currentNoise:
                    return "current noise";
                case port::MessageType::encoderNoise:
                    return "encoder noise";
                default:
                    return "request";
            }
        }
    }

    TorqueNoisePanel::TorqueNoisePanel(RigMotor& motor, const std::optional<sil::SilPlantConfig>& plant, QWidget* parent)
        : QWidget(parent)
        , motor(motor)
        , plant(plant)
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(BuildTorque());
        layout->addWidget(BuildNoise());
        layout->addWidget(BuildActive());
        layout->addWidget(BuildMotor());
        layout->addStretch(1);
        FormChanged();
        Refresh();
    }

    QDoubleSpinBox* TorqueNoisePanel::Spin(double minimum, double maximum, int decimals, const QString& suffix, const QString& name)
    {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(minimum, maximum);
        spin->setDecimals(decimals);
        spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);
        spin->setAccessibleName(name);
        return spin;
    }

    void TorqueNoisePanel::ApplyOnEnter(QDoubleSpinBox& spin, QPushButton& apply)
    {
        spin.installEventFilter(new EnterApplies(apply, &spin));
    }

    QWidget* TorqueNoisePanel::BuildTorque()
    {
        auto* group = new QGroupBox("Torque", this);
        auto* layout = new QFormLayout(group);

        loadTorque = Spin(-10.0, 10.0, 4, " N·m", "Load torque");
        auto* applyLoad = new QPushButton("Apply", group);
        ApplyOnEnter(*loadTorque, *applyLoad);
        layout->addRow("Load", loadTorque);
        layout->addRow("", applyLoad);
        connect(applyLoad, &QPushButton::clicked, this, [this]()
            {
                motor.Send(port::LoadTorque{ static_cast<float>(loadTorque->value()) });
            });

        form = new QComboBox(group);
        form->addItems({ "None", "Constant", "Step", "Ramp", "Sine" });
        form->setAccessibleName("External torque form");
        layout->addRow("External", form);
        auto* applyExternal = new QPushButton("Apply", group);
        for (std::size_t index = 0; index != parameters.size(); ++index)
        {
            parameterLabels[index] = new QLabel(group);
            parameters[index] = Spin(-1000.0, 1000.0, 4, "", "External torque parameter");
            ApplyOnEnter(*parameters[index], *applyExternal);
            layout->addRow(parameterLabels[index], parameters[index]);
        }
        layout->addRow("", applyExternal);
        connect(form, &QComboBox::currentIndexChanged, this, &TorqueNoisePanel::FormChanged);
        connect(applyExternal, &QPushButton::clicked, this, &TorqueNoisePanel::ApplyExternalTorque);
        return group;
    }

    QWidget* TorqueNoisePanel::BuildNoise()
    {
        auto* group = new QGroupBox("Noise", this);
        auto* layout = new QFormLayout(group);

        currentNoise = Spin(0.0, 10.0, 4, " A", "Current noise standard deviation");
        encoderNoise = Spin(0.0, 1.0, 5, " rad", "Encoder noise standard deviation");
        auto* applyCurrent = new QPushButton("Apply", group);
        auto* applyEncoder = new QPushButton("Apply", group);
        ApplyOnEnter(*currentNoise, *applyCurrent);
        ApplyOnEnter(*encoderNoise, *applyEncoder);
        layout->addRow("Current σ", currentNoise);
        layout->addRow("", applyCurrent);
        layout->addRow("Encoder σ", encoderNoise);
        layout->addRow("", applyEncoder);

        connect(applyCurrent, &QPushButton::clicked, this, [this]()
            {
                motor.Send(port::CurrentNoise{ static_cast<float>(currentNoise->value()) });
            });
        connect(applyEncoder, &QPushButton::clicked, this, [this]()
            {
                motor.Send(port::EncoderNoise{ static_cast<float>(encoderNoise->value()) });
            });
        return group;
    }

    QWidget* TorqueNoisePanel::BuildActive()
    {
        auto* group = new QGroupBox("Active changes", this);
        auto* layout = new QVBoxLayout(group);
        inForce = new QLabel(group);
        inForce->setWordWrap(true);
        active = new QListWidget(group);
        active->setAccessibleName("Active changes");
        active->setWordWrap(true);
        active->setMaximumHeight(110);
        remove = new QPushButton("Remove", group);
        remove->setToolTip("Return the selected change to the plant description");
        refusal = new QLabel(group);
        refusal->setWordWrap(true);
        layout->addWidget(inForce);
        layout->addWidget(active);
        layout->addWidget(remove);
        layout->addWidget(refusal);
        connect(remove, &QPushButton::clicked, this, &TorqueNoisePanel::RemoveSelected);
        connect(active, &QListWidget::itemSelectionChanged, this, [this]()
            {
                remove->setEnabled(!active->selectedItems().isEmpty());
            });
        return group;
    }

    QWidget* TorqueNoisePanel::BuildMotor()
    {
        auto* group = new QGroupBox("Motor (read-only)", this);
        auto* layout = new QFormLayout(group);
        if (!plant)
        {
            layout->addRow(new QLabel("The plant description could not be read.", group));
            return group;
        }

        layout->addRow("R", new QLabel(QString("%1 Ω").arg(plant->statorResistanceOhm, 0, 'g', 4), group));
        layout->addRow("Ld", new QLabel(QString("%1 mH").arg(plant->dAxisInductanceHenry * 1000.0, 0, 'g', 4), group));
        layout->addRow("Lq", new QLabel(QString("%1 mH").arg(plant->qAxisInductanceHenry * 1000.0, 0, 'g', 4), group));
        layout->addRow("ψf", new QLabel(QString("%1 Wb").arg(plant->fluxLinkageWeber, 0, 'g', 4), group));
        layout->addRow("Pole pairs", new QLabel(QString::number(plant->polePairs), group));
        layout->addRow("J", new QLabel(QString("%1 kg·m²").arg(plant->rotorInertiaKgM2, 0, 'g', 4), group));
        layout->addRow("B", new QLabel(QString("%1 N·m·s/rad").arg(plant->viscousDampingNmSPerRad, 0, 'g', 4), group));
        auto* note = new QLabel("Another motor means another plant description and a restart.", group);
        note->setWordWrap(true);
        layout->addRow(note);
        return group;
    }

    void TorqueNoisePanel::FormChanged()
    {
        const auto fields = FieldsOf(form->currentIndex());
        const std::array<const char*, 3> names{ fields.first, fields.second, fields.third };
        const bool sine = static_cast<port::TorqueForm>(form->currentIndex()) == port::TorqueForm::sine;
        for (std::size_t index = 0; index != parameters.size(); ++index)
        {
            const bool used = names[index] != nullptr;
            parameterLabels[index]->setVisible(used);
            parameters[index]->setVisible(used);
            if (!used)
                continue;
            parameterLabels[index]->setText(names[index]);
            parameters[index]->setSuffix(index == 2 ? (sine ? " Hz" : " s") : " N·m");
            parameters[index]->setAccessibleName(QString("External torque %1").arg(names[index]).toLower());
        }
    }

    void TorqueNoisePanel::ApplyExternalTorque()
    {
        port::ExternalTorque torque{ static_cast<port::TorqueForm>(form->currentIndex()) };
        for (std::size_t index = 0; index != parameters.size(); ++index)
            torque.parameters[index] = static_cast<float>(parameters[index]->value());
        motor.Send(port::ExternalTorqueRequest{ torque });
    }

    void TorqueNoisePanel::Refresh()
    {
        const auto& changes = motor.Changes();
        if (changes && shown && Same(*changes, *shown))
            return;

        shown = changes;
        active->clear();
        refusal->clear();
        if (!changes)
        {
            inForce->setText("Waiting for the motor to report what is in force…");
            remove->setEnabled(false);
            return;
        }

        ShowInForce(*changes);
        inForce->setText(QString("In force: load %1 N·m · external %2 · current σ %3 A · encoder σ %4 rad")
                             .arg(changes->loadTorque, 0, 'f', 4)
                             .arg(QString::fromStdString(ExternalTorqueText(changes->externalTorque)))
                             .arg(changes->currentNoise, 0, 'f', 4)
                             .arg(changes->encoderNoise, 0, 'f', 5));
        for (const auto& change : ActiveChanges(*changes))
        {
            auto* item = new QListWidgetItem(QString::fromStdString(change.text), active);
            item->setData(Qt::UserRole, static_cast<int>(change.kind));
        }
        if (active->count() == 0)
            new QListWidgetItem("None: the plant is as its description says", active);
        remove->setEnabled(false);
    }

    void TorqueNoisePanel::ShowRefusal(const port::Refused& refused)
    {
        const QString reason = refused.reason == port::RefusalReason::unknownForm ? "the motor does not know that form" : "a value is out of range";
        refusal->setText(QString("Not applied: the %1 change was refused because %2.").arg(Refused(refused.request), reason));
        ui::backend::qt::StyleStatusLabel(*refusal, ui::theme::StatusLevel::Warning);
    }

    void TorqueNoisePanel::RemoveSelected()
    {
        const auto selected = active->selectedItems();
        if (selected.isEmpty() || !selected.front()->data(Qt::UserRole).isValid())
            return;

        switch (static_cast<ChangeKind>(selected.front()->data(Qt::UserRole).toInt()))
        {
            case ChangeKind::loadTorque:
                motor.Send(port::LoadTorque{ plant ? plant->loadTorqueNm : 0.0f });
                break;
            case ChangeKind::externalTorque:
                motor.Send(port::ExternalTorqueRequest{});
                break;
            case ChangeKind::currentNoise:
                motor.Send(port::CurrentNoise{ plant ? plant->noiseSigmaAmpere : 0.0f });
                break;
            case ChangeKind::encoderNoise:
                motor.Send(port::EncoderNoise{ plant ? plant->encoderSigmaRadians : 0.0f });
                break;
        }
    }

    void TorqueNoisePanel::ShowInForce(const port::Changes& changes)
    {
        const auto set = [](QDoubleSpinBox& spin, float value)
        {
            if (!spin.hasFocus())
                spin.setValue(value);
        };
        set(*loadTorque, changes.loadTorque);
        set(*currentNoise, changes.currentNoise);
        set(*encoderNoise, changes.encoderNoise);
        if (form->hasFocus())
            return;

        form->setCurrentIndex(static_cast<int>(changes.externalTorque.form));
        for (std::size_t index = 0; index != parameters.size(); ++index)
            set(*parameters[index], changes.externalTorque.parameters[index]);
    }
}
