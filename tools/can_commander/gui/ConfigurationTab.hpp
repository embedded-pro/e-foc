#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"

namespace tool
{
    class ConfigurationTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit ConfigurationTab(QWidget* parent = nullptr);

        void SetLegacy(bool legacyContract) override;
    };
}
