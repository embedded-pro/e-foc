#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"
#include <QLabel>

namespace tool
{
    class ConfigurationTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit ConfigurationTab(QWidget* parent = nullptr);

    protected:
        void Refresh() override;

    private:
        void BuildStartupGroup();
        void BuildStreamGroup();
        void BuildSupervisionGroup();
        void BuildLimitsGroup();
        void BuildMotionProfileGroup();
        void BuildTargetWindowGroup();
        void BuildTravelLimitsGroup();
        void BuildThermalGroup();
        void BuildSupplyLimitsGroup();
        void BuildRecalibrationGroup();

        QLabel* stallWarning{ nullptr };
    };
}
