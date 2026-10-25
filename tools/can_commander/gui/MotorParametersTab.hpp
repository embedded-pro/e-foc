#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace tool
{
    // The motor model the controllers are designed from, and the commissioning actions that measure it
    class MotorParametersTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit MotorParametersTab(QWidget* parent = nullptr);

        void SetLegacy(bool legacyContract) override;
        void SetActionsAvailable(const QString& reason);
        void SetMechanicalIdentificationUnavailable(const QString& reason);

    signals:
        void AlignRequested();
        void IdentifyElectricalRequested();
        void IdentifyMechanicalRequested();

    protected:
        void Refresh() override;

    private:
        QGroupBox* CreateCommissioningGroup();

        QLabel* polePairsHint{ nullptr };
        QPushButton* alignButton{ nullptr };
        QPushButton* identifyElectricalButton{ nullptr };
        QPushButton* identifyMechanicalButton{ nullptr };
        QString actionsUnavailable;
        QString mechanicalUnavailable;
    };
}
