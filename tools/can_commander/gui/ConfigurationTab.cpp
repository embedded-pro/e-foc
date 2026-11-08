#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"

namespace tool
{
    ConfigurationTab::ConfigurationTab(QWidget* parent)
        : SettingsPage(parent)
    {
        BuildStartupGroup();
        BuildStreamGroup();
        BuildSupervisionGroup();
        BuildLimitsGroup();
        BuildTargetWindowGroup();
        BuildTravelLimitsGroup();
        BuildThermalGroup();
        BuildSupplyLimitsGroup();
        BuildRecalibrationGroup();
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

    void ConfigurationTab::BuildStreamGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Current Stream", group);
        AddEditors(form, StreamSettings());

        auto* note = new QLabel("Stored at once and in force within 1 ms, also while the motor runs; 0 stops the stream. Every "
                                "controller's stream shares the bus: keep the rates of all of them together within its bitrate.");
        note->setWordWrap(true);
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

        AddEditors(form, HomingSettings());
        auto* homingNote = new QLabel("With homing required, position mode starts only once a calibration or the encoder index "
                                      "has set the commissioned zero in this power-up. Speed and torque mode are not held back.");
        homingNote->setWordWrap(true);
        form.addRow(homingNote);
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

    void ConfigurationTab::BuildRecalibrationGroup()
    {
        QGroupBox* group{ nullptr };
        auto& form = AddGroup("Online Recalibration", group);
        AddEditors(form, RecalibrationSettings());

        auto* note = new QLabel("Speed and position mode re-estimate the motor while it runs. On command redesigns the loops from "
                                "the settled estimates when Recalibrate is pressed on the Diagnostics tab; Automatic does it at every "
                                "stop. The loops never change while the motor runs. Stored at once.");
        note->setWordWrap(true);
        form.addRow(note);
    }

    void ConfigurationTab::Refresh()
    {
        const auto* stall = EditorFor(can::FocParameter::stallDetection);
        stallWarning->setVisible(stall->IsAvailable() && stall->Held().has_value() && stall->EditedValue() < 0.5f);

        SettingsPage::Refresh();
    }
}
