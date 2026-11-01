#pragma once

#include "core/bootloader/StatusJournal.hpp"
#include "upgrade/boot_loader/PackUpgrader.hpp"
#include "upgrade/boot_loader/UpgradePackLoader.hpp"

namespace bootloader
{
    class JournalPackLoader
        : public application::UpgradePackLoader
    {
    public:
        JournalPackLoader(hal::SynchronousFlash& packFlash, const char* product, StatusJournal& journal);

    protected:
        application::UpgradePackStatus ReadStatus() override;
        void WriteStatus(application::UpgradePackStatus status) override;
        void WriteError(uint32_t errorCode) override;

    private:
        StatusJournal& journal;
    };

    class JournalPackUpgrader
        : public application::PackUpgrader
    {
    public:
        JournalPackUpgrader(hal::SynchronousFlash& packFlash, StatusJournal& journal);

    protected:
        void WriteStatus(application::UpgradePackStatus status) override;
        void WriteError(uint32_t errorCode) override;

    private:
        StatusJournal& journal;
    };
}
