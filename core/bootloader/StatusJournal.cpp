#include "core/bootloader/StatusJournal.hpp"
#include "infra/util/ByteRange.hpp"
#include <algorithm>

namespace bootloader
{
    namespace
    {
        constexpr uint32_t Check(uint32_t marker, uint8_t status, uint32_t errorCode)
        {
            return ~(marker ^ status ^ errorCode);
        }

        bool IsErased(const StatusJournalEntry& entry)
        {
            const auto bytes = infra::MakeByteRange(entry);
            return std::all_of(bytes.begin(), bytes.end(), [](uint8_t byte)
                {
                    return byte == 0xff;
                });
        }
    }

    StatusJournalEntry MakeStatusJournalEntry(application::UpgradePackStatus status, uint32_t errorCode)
    {
        const auto statusByte = static_cast<uint8_t>(status);
        return StatusJournalEntry{ StatusJournalEntry::validMarker, statusByte, { 0xff, 0xff, 0xff }, errorCode, Check(StatusJournalEntry::validMarker, statusByte, errorCode) };
    }

    bool IsValid(const StatusJournalEntry& entry)
    {
        return entry.marker == StatusJournalEntry::validMarker && entry.check == Check(entry.marker, entry.status, entry.errorCode);
    }

    StatusJournal::StatusJournal(hal::SynchronousFlash& flash)
        : flash(flash)
    {}

    application::UpgradePackStatus StatusJournal::Status() const
    {
        const auto scan = Read();
        return scan.latest ? static_cast<application::UpgradePackStatus>(scan.latest->status) : application::UpgradePackStatus::empty;
    }

    uint32_t StatusJournal::ErrorCode() const
    {
        const auto scan = Read();
        return scan.latest ? scan.latest->errorCode : 0;
    }

    void StatusJournal::Record(application::UpgradePackStatus status, uint32_t errorCode)
    {
        const auto scan = Read();
        if (scan.latest && scan.latest->status == static_cast<uint8_t>(status) && scan.latest->errorCode == errorCode)
            return;

        auto address = scan.firstErased;
        if (!address)
        {
            Clear();
            address = 0;
        }

        const auto entry = MakeStatusJournalEntry(status, errorCode);
        flash.WriteBuffer(infra::MakeByteRange(entry), *address);
    }

    void StatusJournal::Clear()
    {
        flash.EraseAll();
    }

    StatusJournal::Scan StatusJournal::Read() const
    {
        Scan scan;

        for (uint32_t address = 0; address + sizeof(StatusJournalEntry) <= flash.TotalSize(); address += sizeof(StatusJournalEntry))
        {
            const auto entry = ReadEntry(address);
            if (IsErased(entry))
            {
                scan.firstErased = address;
                break;
            }

            if (IsValid(entry))
                scan.latest = entry;
        }

        return scan;
    }

    StatusJournalEntry StatusJournal::ReadEntry(uint32_t address) const
    {
        StatusJournalEntry entry{};
        flash.ReadBuffer(infra::MakeByteRange(entry), address);
        return entry;
    }
}
