#pragma once

#include <cstdint>

namespace services
{
    struct FirmwareVersion
    {
        uint16_t major{};
        uint16_t minor{};
        uint16_t patch{};

        bool operator==(const FirmwareVersion& other) const = default;
    };

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
