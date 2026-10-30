#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <algorithm>

namespace tool
{
    namespace
    {
        bool IsSupervisionSetting(can::FocParameter parameter)
        {
            return std::ranges::any_of(SupervisionSettings(), [parameter](const SettingPresentation& setting)
                {
                    return setting.parameter == parameter;
                });
        }
    }

    ConfigurationTab::ConfigurationTab(QWidget* parent)
        : SettingsPage(parent)
    {
        BuildStartupGroup();
        BuildSupervisionGroup();
        Content().addStretch();

        Refresh();
    }

    void ConfigurationTab::BuildStartupGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Start-up Settings", group);
        AddEditors(form, StartupSettings());

        auto* note = new QLabel("Stored at once and used from the controller's next start. After a new node ID the motor "
                                "appears under another tab; after a new bitrate, reconnect at that bitrate to reach it.");
        note->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*note, ui::theme::StatusLevel::Warning);
        form.addRow(note);
    }

    void ConfigurationTab::BuildSupervisionGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Fault Supervision", group);
        AddEditors(form, SupervisionSettings());

        auto* note = new QLabel("One set for every control mode, stored at once and in force within 10 ms, also while "
                                "the motor runs. An encoder that stops reporting is always a sensor fault.");
        note->setWordWrap(true);
        form.addRow(note);

        stallWarning = new QLabel("Stall reporting is off: a rotor held against the drive can carry up to the current "
                                  "limit with no fault, and only the board's thermal protection watches it.");
        stallWarning->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*stallWarning, ui::theme::StatusLevel::Warning);
        stallWarning->setVisible(false);
        form.addRow(stallWarning);
    }

    void ConfigurationTab::SetLegacy(bool legacyContract)
    {
        legacyReason = legacyContract ? QString("Needs firmware contract 2.1 or newer: this controller has 2.0.") : QString{};
        ShowAvailability();

        SettingsPage::SetLegacy(legacyContract);
    }

    void ConfigurationTab::SetSupervisionUnavailable(const QString& reason)
    {
        if (reason == supervisionReason)
            return;

        supervisionReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::Refresh()
    {
        const auto* stall = EditorFor(can::FocParameter::stallDetection);
        stallWarning->setVisible(stall->IsAvailable() && stall->Held().has_value() && stall->EditedValue() < 0.5f);

        SettingsPage::Refresh();
    }

    void ConfigurationTab::ShowAvailability()
    {
        for (auto* editor : Editors())
            if (IsSupervisionSetting(editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? supervisionReason : legacyReason);
            else if (editor->Parameter() != can::FocParameter::encoderResolution)
                editor->SetUnavailable(legacyReason);
    }
}
