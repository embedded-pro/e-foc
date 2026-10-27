#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "tools/can_commander/gui/ControllersTab.hpp"
#include "tools/can_commander/gui/DiagnosticsTab.hpp"
#include "tools/can_commander/gui/MotorParametersTab.hpp"
#include "tools/can_commander/gui/SetpointKnob.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <optional>
#include <vector>

namespace tool
{
    class CommandPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit CommandPanel(QWidget* parent = nullptr);

        static QString ControlModeName(can::FocMotorMode mode);

        ControllersTab& Controllers() const;
        MotorParametersTab& MotorParameters() const;
        ConfigurationTab& Configuration() const;
        DiagnosticsTab& Diagnostics() const;
        std::array<SettingsPage*, 3> SettingsPages() const;
        std::optional<can::FocMotorMode> ActiveControlMode() const;

    signals:
        void StartMotorRequested();
        void StopMotorRequested();
        void ClearFaultRequested();
        void SetControlModeRequested(can::FocMotorMode mode);
        void SetpointRequested(can::FocMotorMode mode, int32_t counts);

        void RequestDataRequested();

    public slots:
        void SetCommandsAvailable(const QString& unavailableReason, bool explained);
        void SetSettingsAvailable(const QString& unavailableReason);
        void ShowRequestedControlMode(can::FocMotorMode mode);
        void SetActiveControlMode(can::FocMotorMode mode);
        void ClearActiveControlMode();
        void ShowAcceptedSetpoint(can::FocMotorMode mode, int32_t counts);
        void ForgetAcceptedSetpoints();
        void ShowMotorParameters();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct SetpointControls
        {
            can::FocMotorMode mode{ can::FocMotorMode::torque };
            SetpointKnob* knob{ nullptr };
            QDoubleSpinBox* spin{ nullptr };
            QPushButton* apply{ nullptr };
            QLabel* status{ nullptr };
            std::optional<int32_t> accepted;
        };

        QWidget* CreateMotorControlTab();
        QGroupBox* CreateControlGroup();
        QGroupBox* CreateModeGroup();
        QStackedWidget* CreateSetpointStack();
        QGroupBox* CreateSetpointGroup(SetpointControls& controls, const char* title, const char* label);
        QLayout* CreateSetpointEntry(SetpointControls& controls, const char* title, const char* label, const QString& range);
        void ConnectSetpoint(SetpointControls& controls);
        void SyncSetpoint(SetpointControls& controls, int32_t counts);
        void RefreshSetpointStatus(SetpointControls& controls);

        QLabel* hintLabel;
        QTabWidget* tabs;
        QStackedWidget* setpointStack;
        QLabel* modeLabel;
        ControllersTab* controllers;
        MotorParametersTab* motorParameters;
        ConfigurationTab* configuration;
        DiagnosticsTab* diagnostics;
        std::vector<QWidget*> commandWidgets;
        std::array<SetpointControls, 3> setpoints{};
        std::optional<can::FocMotorMode> activeMode;
    };
}
