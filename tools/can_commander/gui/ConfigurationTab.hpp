#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"
#include <QLabel>
#include <QString>

namespace tool
{
    class ConfigurationTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit ConfigurationTab(QWidget* parent = nullptr);

        void SetLegacy(bool legacyContract) override;
        void SetSupervisionUnavailable(const QString& reason);
        void SetLimitsUnavailable(const QString& reason);
        void SetTargetWindowUnavailable(const QString& reason);
        void SetTravelLimitsUnavailable(const QString& reason);

    protected:
        void Refresh() override;

    private:
        void BuildStartupGroup();
        void BuildSupervisionGroup();
        void BuildLimitsGroup();
        void BuildTargetWindowGroup();
        void BuildTravelLimitsGroup();
        void ShowAvailability();

        QLabel* stallWarning{ nullptr };
        QString legacyReason;
        QString supervisionReason;
        QString limitsReason;
        QString targetWindowReason;
        QString travelLimitsReason;
    };
}
