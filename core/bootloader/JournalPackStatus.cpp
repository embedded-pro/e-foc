#include "core/bootloader/JournalPackStatus.hpp"

namespace bootloader
{
    JournalPackLoader::JournalPackLoader(hal::SynchronousFlash& packFlash, const char* product, StatusJournal& journal)
        : application::UpgradePackLoader(packFlash, product)
        , journal(journal)
    {}

    application::UpgradePackStatus JournalPackLoader::ReadStatus()
    {
        return journal.Status();
    }

    void JournalPackLoader::WriteStatus(application::UpgradePackStatus status)
    {
        journal.Record(status, 0);
    }

    void JournalPackLoader::WriteError(uint32_t errorCode)
    {
        journal.Record(journal.Status(), errorCode);
    }

    JournalPackUpgrader::JournalPackUpgrader(hal::SynchronousFlash& packFlash, StatusJournal& journal)
        : application::PackUpgrader(packFlash)
        , journal(journal)
    {}

    void JournalPackUpgrader::WriteStatus(application::UpgradePackStatus status)
    {
        journal.Record(status, 0);
    }

    void JournalPackUpgrader::WriteError(uint32_t errorCode)
    {
        journal.Record(journal.Status(), errorCode);
    }
}
