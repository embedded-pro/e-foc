#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"

namespace tool
{
    ConfigurationTab::ConfigurationTab(QWidget* parent)
        : SettingsPage(parent)
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Start-up Settings", group);
        AddEditors(form, StartupSettings());

        auto* note = new QLabel("Stored at once and used from the controller's next start. After a new node ID the motor "
                                "appears under another tab; after a new bitrate, reconnect at that bitrate to reach it.");
        note->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*note, ui::theme::StatusLevel::Warning);
        form.addRow(note);
        Content().addStretch();

        Refresh();
    }

    // Contract 2.0 sets the encoder resolution with its own message and has none for the others
    void ConfigurationTab::SetLegacy(bool legacyContract)
    {
        for (auto* editor : Editors())
            if (editor->Parameter() != can::FocParameter::encoderResolution)
                editor->SetUnavailable(legacyContract ? QString("Needs firmware contract 2.1 or newer: this controller has 2.0.") : QString{});

        SettingsPage::SetLegacy(legacyContract);
    }
}
