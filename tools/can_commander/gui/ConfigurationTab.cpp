#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <algorithm>
#include <span>

namespace tool
{
    namespace
    {
        bool IsIn(std::span<const SettingPresentation> settings, can::FocParameter parameter)
        {
            return std::ranges::any_of(settings, [parameter](const SettingPresentation& setting)
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
        BuildLimitsGroup();
        BuildTargetWindowGroup();
        BuildTravelLimitsGroup();
        BuildThermalGroup();
        BuildSupplyLimitsGroup();
        BuildQuickStopGroup();
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

    void ConfigurationTab::BuildLimitsGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Limits", group);
        AddEditors(form, LimitSettings());

        auto* note = new QLabel("One set for every control mode, stored at once and in force within 1 ms, also while the motor "
                                "runs. Above 110 % of the overspeed limit, or beyond the following-error window for its time in "
                                "position mode, the drive faults and coasts.");
        note->setWordWrap(true);
        form.addRow(note);
    }

    void ConfigurationTab::BuildTargetWindowGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Target Window", group);
        AddEditors(form, TargetWindowSettings());

        auto* note = new QLabel("Position mode reports its target reached once the position error has stayed within the "
                                "window for the window time; a window of 0 keeps it reported not reached. Stored at once "
                                "and in force within 1 ms, also while the motor runs.");
        note->setWordWrap(true);
        form.addRow(note);
    }

    void ConfigurationTab::BuildTravelLimitsGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Travel Limits", group);
        AddEditors(form, TravelLimitSettings());

        auto* note = new QLabel("Measured from the alignment zero, and off while the minimum is not below the maximum. When "
                                "on, position mode refuses a setpoint outside them and moves the direct way between them, "
                                "never across the angle outside them. Stored at once and in force from the next control period.");
        note->setWordWrap(true);
        form.addRow(note);
    }

    void ConfigurationTab::BuildThermalGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Thermal", group);
        AddEditors(form, ThermalSettings());
        AddEditors(form, OverTemperatureSettings());

        auto* note = new QLabel("The board has no temperature sensor: speed and position mode infer the winding temperature from "
                                "the online resistance estimate, starting from the winding's temperature when the calibration "
                                "measured its resistance, derate the current over the margin below the limit and fault at the "
                                "limit. Torque mode has no estimate and none of this. Stored at once.");
        note->setWordWrap(true);
        form.addRow(note);
    }

    void ConfigurationTab::BuildSupplyLimitsGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Supply Limits", group);
        AddEditors(form, BusVoltageSettings());

        auto* note = new QLabel("Each limit is off at 0. A running motor whose bus voltage, filtered over 50 ms, leaves them faults "
                                "with undervoltage or overvoltage and coasts; a stopped motor is not watched. The board's own "
                                "voltage trips stay in force. Stored at once and in force within 10 ms.");
        note->setWordWrap(true);
        form.addRow(note);
    }

    void ConfigurationTab::BuildQuickStopGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Quick Stop", group);
        AddEditors(form, QuickStopSettings());

        auto* note = new QLabel("Quick Stop decelerates a running motor at this rate and stops it once it is below 2 rad/s, at most a "
                                "second later; 0 brakes at the current limit. Torque mode without a calibrated inertia stops at once. "
                                "Stored at once and used by the next quick stop.");
        note->setWordWrap(true);
        form.addRow(note);
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

    void ConfigurationTab::SetLimitsUnavailable(const QString& reason)
    {
        if (reason == limitsReason)
            return;

        limitsReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::SetThermalUnavailable(const QString& reason)
    {
        if (reason == thermalReason)
            return;

        thermalReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::SetOverTemperatureUnavailable(const QString& reason)
    {
        if (reason == overTemperatureReason)
            return;

        overTemperatureReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::SetBusVoltageLimitsUnavailable(const QString& reason)
    {
        if (reason == busVoltageLimitsReason)
            return;

        busVoltageLimitsReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::SetQuickStopUnavailable(const QString& reason)
    {
        if (reason == quickStopReason)
            return;

        quickStopReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::SetTravelLimitsUnavailable(const QString& reason)
    {
        if (reason == travelLimitsReason)
            return;

        travelLimitsReason = reason;
        ShowAvailability();
        Refresh();
    }

    void ConfigurationTab::SetTargetWindowUnavailable(const QString& reason)
    {
        if (reason == targetWindowReason)
            return;

        targetWindowReason = reason;
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
            if (IsIn(SupervisionSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? supervisionReason : legacyReason);
            else if (IsIn(LimitSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? limitsReason : legacyReason);
            else if (IsIn(TargetWindowSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? targetWindowReason : legacyReason);
            else if (IsIn(TravelLimitSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? travelLimitsReason : legacyReason);
            else if (IsIn(ThermalSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? thermalReason : legacyReason);
            else if (IsIn(OverTemperatureSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? overTemperatureReason : legacyReason);
            else if (IsIn(BusVoltageSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? busVoltageLimitsReason : legacyReason);
            else if (IsIn(QuickStopSettings(), editor->Parameter()))
                editor->SetUnavailable(legacyReason.isEmpty() ? quickStopReason : legacyReason);
            else if (editor->Parameter() != can::FocParameter::encoderResolution)
                editor->SetUnavailable(legacyReason);
    }
}
