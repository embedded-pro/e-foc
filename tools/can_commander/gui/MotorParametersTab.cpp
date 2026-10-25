#include "tools/can_commander/gui/MotorParametersTab.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace tool
{
    MotorParametersTab::MotorParametersTab(QWidget* parent)
        : SettingsPage(parent)
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Motor Parameters", group);
        AddEditors(form, MotorParameterSettings());

        polePairsHint = new QLabel("After a pole-pair change the rotor has to be aligned again before the motor can start.");
        polePairsHint->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*polePairsHint, ui::theme::StatusLevel::Warning);
        form.addRow(polePairsHint);

        Content().addWidget(CreateCommissioningGroup());
        Content().addStretch();

        Refresh();
    }

    QGroupBox* MotorParametersTab::CreateCommissioningGroup()
    {
        auto* group = new QGroupBox("Commissioning");
        auto* layout = new QVBoxLayout(group);

        auto* warning = new QLabel("These actions move the rotor: keep the shaft free and clear of people. "
                                   "Each one answers when it finishes, which can take up to two minutes.");
        warning->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*warning, ui::theme::StatusLevel::Warning);
        layout->addWidget(warning);

        alignButton = new QPushButton("A&lign Rotor");
        alignButton->setToolTip("Find the rotor's electrical zero. Needed after a pole-pair change.");
        identifyElectricalButton = new QPushButton("Identify &Electrical");
        identifyElectricalButton->setToolTip("Measure resistance, inductance and pole pairs, then align the rotor");
        identifyMechanicalButton = new QPushButton("Identify Mecha&nical");
        identifyMechanicalButton->setToolTip("Measure inertia and viscous friction by spinning the rotor");

        auto* buttons = new QHBoxLayout();
        buttons->addWidget(alignButton);
        buttons->addWidget(identifyElectricalButton);
        buttons->addWidget(identifyMechanicalButton);
        buttons->addStretch();
        layout->addLayout(buttons);

        connect(alignButton, &QPushButton::clicked, this, &MotorParametersTab::AlignRequested);
        connect(identifyElectricalButton, &QPushButton::clicked, this, &MotorParametersTab::IdentifyElectricalRequested);
        connect(identifyMechanicalButton, &QPushButton::clicked, this, &MotorParametersTab::IdentifyMechanicalRequested);
        return group;
    }

    // Contract 2.0 has no message for the motor parameters; its commissioning actions still work
    void MotorParametersTab::SetLegacy(bool legacyContract)
    {
        for (auto* editor : Editors())
            editor->SetUnavailable(legacyContract ? QString("Needs firmware contract 2.1 or newer: this controller has 2.0.") : QString{});

        SettingsPage::SetLegacy(legacyContract);
    }

    void MotorParametersTab::SetActionsAvailable(const QString& reason)
    {
        actionsUnavailable = reason;
        Refresh();
    }

    void MotorParametersTab::SetMechanicalIdentificationUnavailable(const QString& reason)
    {
        mechanicalUnavailable = reason;
        Refresh();
    }

    void MotorParametersTab::Refresh()
    {
        polePairsHint->setVisible(EditorFor(can::FocParameter::polePairs)->IsEdited());

        const bool available = actionsUnavailable.isEmpty();
        alignButton->setEnabled(available);
        alignButton->setToolTip(available ? QString("Find the rotor's electrical zero. Needed after a pole-pair change.") : actionsUnavailable);
        identifyElectricalButton->setEnabled(available);
        identifyElectricalButton->setToolTip(available ? QString("Measure resistance, inductance and pole pairs, then align the rotor") : actionsUnavailable);

        const auto& mechanicalReason = available ? mechanicalUnavailable : actionsUnavailable;
        identifyMechanicalButton->setEnabled(mechanicalReason.isEmpty());
        identifyMechanicalButton->setToolTip(mechanicalReason.isEmpty() ? QString("Measure inertia and viscous friction by spinning the rotor") : mechanicalReason);

        SettingsPage::Refresh();
    }
}
