#pragma once

#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QWidget>
#include <vector>

namespace tool
{
    class CommandPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit CommandPanel(QWidget* parent = nullptr);

    signals:
        void StartMotorRequested();
        void StopMotorRequested();
        void EmergencyStopRequested();
        void SetControlModeRequested(services::CanControlMode mode);
        void SetTorqueSetpointRequested(float idCurrent, float iqCurrent);
        void SetSpeedSetpointRequested(float speedRadPerSec);
        void SetPositionSetpointRequested(float positionRad);

        void SetCurrentIdPidRequested(float kp, float ki, float kd);
        void SetCurrentIqPidRequested(float kp, float ki, float kd);
        void SetSpeedPidRequested(float kp, float ki, float kd);
        void SetPositionPidRequested(float kp, float ki, float kd);

        void SetSupplyVoltageRequested(float volts);
        void SetMaxCurrentRequested(float amps);

        void RequestDataRequested();

    public slots:
        void SetCommandsEnabled(bool enabled);
        void SetActiveControlMode(services::CanControlMode mode);

    private:
        QWidget* CreateMotorControlTab();
        QWidget* CreatePidTuningTab();
        QWidget* CreateParametersTab();

        QTabWidget* tabs;
        QStackedWidget* setpointStack;
        QStackedWidget* pidStack;
        QLabel* modeLabel;
        std::vector<QPushButton*> commandButtons;
    };
}
