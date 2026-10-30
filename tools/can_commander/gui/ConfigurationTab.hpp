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

    protected:
        void Refresh() override;

    private:
        void BuildStartupGroup();
        void BuildSupervisionGroup();
        void ShowAvailability();

        QLabel* stallWarning{ nullptr };
        QString legacyReason;
        QString supervisionReason;
    };
}
