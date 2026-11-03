#pragma once

namespace services
{
    class FirmwareUpgradeDrive
    {
    protected:
        FirmwareUpgradeDrive() = default;
        FirmwareUpgradeDrive(const FirmwareUpgradeDrive& other) = delete;
        FirmwareUpgradeDrive& operator=(const FirmwareUpgradeDrive& other) = delete;
        ~FirmwareUpgradeDrive() = default;

    public:
        virtual bool IsStoppedAndIdle() const = 0;
        virtual void Hold() = 0;
        virtual void Release() = 0;
    };
}
