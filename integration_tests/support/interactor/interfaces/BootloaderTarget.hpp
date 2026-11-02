#pragma once

#include <cstdint>
#include <vector>

namespace integration
{
    // The emulated board's layout (documentation/design/software-in-the-loop.md, Part P)
    inline constexpr uint32_t emulatedSlotStart = 0x00008000;
    inline constexpr uint32_t emulatedSlotSize = 256 * 1024;
    inline constexpr uint32_t emulatedJournalSize = 16 * 1024;

    class BootloaderTarget
    {
    public:
        enum class Slot
        {
            application,
            damagedApplication,
            empty,
        };

        virtual std::vector<uint8_t> ApplicationImage() const = 0;
        virtual std::vector<uint8_t> DevelopmentKeySet() const = 0;

        virtual void ConfigureSlot(Slot slot) = 0;
        virtual void ConfigureStaging(const std::vector<uint8_t>& pack) = 0;
        virtual void ConfigureJournal(const std::vector<uint8_t>& journal) = 0;
        virtual std::vector<uint8_t> Journal() const = 0;

        static BootloaderTarget* Instance();
        static void SetInstance(BootloaderTarget& instance);

    protected:
        ~BootloaderTarget() = default;
    };
}
