#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "tools/can_commander/gui/ControllersTab.hpp"
#include "tools/can_commander/gui/MotorParametersTab.hpp"
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
        std::array<SettingsPage*, 3> SettingsPages() const;

    signals:
        void StartMotorRequested();
        void StopMotorRequested();
        void ClearFaultRequested();
        void SetControlModeRequested(can::FocMotorMode mode);
        void SetTorqueSetpointRequested(float iqCurrent);
        void SetSpeedSetpointRequested(float speedRadPerSec);
        void SetPositionSetpointRequested(float positionRad);

        void RequestDataRequested();

    public slots:
        void SetCommandsAvailable(const QString& unavailableReason, bool explained);
        void SetSettingsAvailable(const QString& unavailableReason);
        void ShowRequestedControlMode(can::FocMotorMode mode);
        void SetActiveControlMode(can::FocMotorMode mode);
        void ClearActiveControlMode();

    private:
        QWidget* CreateMotorControlTab();
        QGroupBox* CreateControlGroup();
        QGroupBox* CreateModeGroup();
        QStackedWidget* CreateSetpointStack();
        QGroupBox* CreateSetpointGroup(const char* title, const char* label, double limit, int decimals, const char* suffix, void (CommandPanel::*signal)(float));

        QLabel* hintLabel;
        QTabWidget* tabs;
        QStackedWidget* setpointStack;
        QLabel* modeLabel;
        ControllersTab* controllers;
        MotorParametersTab* motorParameters;
        ConfigurationTab* configuration;
        std::vector<QWidget*> commandWidgets;
    };
}
