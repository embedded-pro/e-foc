#pragma once

#include "core/services/firmware_upgrade/FirmwareVersion.hpp"

namespace services
{
    class FirmwareUpgradeControl
    {
    protected:
        FirmwareUpgradeControl() = default;
        FirmwareUpgradeControl(const FirmwareUpgradeControl& other) = delete;
        FirmwareUpgradeControl& operator=(const FirmwareUpgradeControl& other) = delete;
        ~FirmwareUpgradeControl() = default;

    public:
        virtual bool IsRunning() const = 0;
        virtual FirmwareVersion StagedVersion() const = 0;
        virtual void EmergencyStop() = 0;
    };
}
