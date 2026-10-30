#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace tool
{
    class MotorParametersTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit MotorParametersTab(QWidget* parent = nullptr);

        void SetLegacy(bool legacyContract) override;
        void SetActionsAvailable(const QString& reason);
        void SetMaintenanceUnavailable(const QString& reason);

    signals:
        void AlignRequested();
        void IdentifyElectricalRequested();
        void CalibrateRequested();
        void ClearCalibrationRequested();

    protected:
        void Refresh() override;

    private:
        QGroupBox* CreateCommissioningGroup();

        QLabel* polePairsHint{ nullptr };
        QPushButton* alignButton{ nullptr };
        QPushButton* identifyElectricalButton{ nullptr };
        QPushButton* calibrateButton{ nullptr };
        QPushButton* clearCalibrationButton{ nullptr };
        QString actionsUnavailable;
        QString maintenanceUnavailable;
    };
}
