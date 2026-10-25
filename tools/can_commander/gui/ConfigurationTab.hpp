#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"

namespace tool
{
    // Settings the controller reads once at start-up: stored at once, in force after a restart
    class ConfigurationTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit ConfigurationTab(QWidget* parent = nullptr);

        void SetLegacy(bool legacyContract) override;
    };
}
