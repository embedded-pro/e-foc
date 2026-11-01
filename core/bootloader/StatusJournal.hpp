#pragma once

#include "hal/synchronous_interfaces/SynchronousFlash.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace bootloader
{
    struct StatusJournalEntry
    {
        static constexpr uint32_t validMarker = 0x314a4645;

        uint32_t marker;
        uint8_t status;
        std::array<uint8_t, 3> reserved;
        uint32_t errorCode;
        uint32_t check;
    };

    static_assert(sizeof(StatusJournalEntry) == 16);

    StatusJournalEntry MakeStatusJournalEntry(application::UpgradePackStatus status, uint32_t errorCode);
    bool IsValid(const StatusJournalEntry& entry);

    class StatusJournal
    {
    public:
        explicit StatusJournal(hal::SynchronousFlash& flash);

        application::UpgradePackStatus Status() const;
        uint32_t ErrorCode() const;

        void Record(application::UpgradePackStatus status, uint32_t errorCode);
        void Clear();

    private:
        struct Scan
        {
            std::optional<StatusJournalEntry> latest;
            std::optional<uint32_t> firstErased;
        };

        Scan Read() const;
        StatusJournalEntry ReadEntry(uint32_t address) const;

        hal::SynchronousFlash& flash;
    };
}
