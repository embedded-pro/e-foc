#pragma once

#include "core/can/FocMotorMessages.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
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

    signals:
        void StartMotorRequested();
        void StopMotorRequested();
        void SetControlModeRequested(can::FocMotorMode mode);
        void SetTorqueSetpointRequested(float iqCurrent);
        void SetSpeedSetpointRequested(float speedRadPerSec);
        void SetPositionSetpointRequested(float positionRad);

        void SetCurrentBandwidthRequested(float bandwidth);
        void SetSpeedBandwidthRequested(float bandwidth);
        void SetPositionBandwidthRequested(float bandwidth);

        void RequestDataRequested();

    public slots:
        // An empty reason enables the commands. Otherwise the reason becomes their tooltip and, when
        // explained, an inline hint; transient reasons skip the hint so the layout does not jump.
        void SetCommandsAvailable(const QString& unavailableReason, bool explained);
        void ShowRequestedControlMode(can::FocMotorMode mode);
        void SetActiveControlMode(can::FocMotorMode mode);
        void ClearActiveControlMode();

    private:
        QWidget* CreateMotorControlTab();
        QWidget* CreatePidTuningTab();
        QGroupBox* CreateControlGroup();
        QGroupBox* CreateModeGroup();
        QStackedWidget* CreateSetpointStack();
        QGroupBox* CreateSetpointGroup(const char* title, const char* label, double limit, int decimals, const char* suffix, void (CommandPanel::*signal)(float));
        void AddBandwidthGroup(QVBoxLayout& layout, const char* title, float maxBandwidth, void (CommandPanel::*signal)(float));

        QLabel* hintLabel;
        QTabWidget* tabs;
        QStackedWidget* setpointStack;
        QStackedWidget* pidStack;
        QLabel* modeLabel;
        std::vector<QWidget*> commandWidgets;
    };
}
