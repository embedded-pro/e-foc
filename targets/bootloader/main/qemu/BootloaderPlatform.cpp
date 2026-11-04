#include "BootloaderPlatform.hpp"
#include "hal/cortex_m/JumpToApplication.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>

namespace bootloader
{
    namespace
    {
        constexpr uint32_t eraseUnit = 16 * 1024;

        std::array<uint8_t, 256 * 1024> packStorage;
        std::array<uint8_t, 16 * 1024> journalStorage;

        infra::ByteRange Writable(infra::ConstByteRange region)
        {
            return infra::ByteRange{ const_cast<uint8_t*>(region.begin()), const_cast<uint8_t*>(region.end()) };
        }
    }

    BootloaderPlatform::BootloaderPlatform(infra::ConstByteRange packRegion, infra::ConstByteRange journalRegion, infra::ConstByteRange slotRegion)
        : packFlash("staging.bin", infra::MakeRange(packStorage), eraseUnit)
        , journalFlash("journal.bin", infra::MakeRange(journalStorage), eraseUnit)
        , slotFlash(Writable(slotRegion), eraseUnit)
    {
        really_assert(packRegion.size() == packStorage.size() && journalRegion.size() == journalStorage.size());
    }

    hal::SynchronousFlash& BootloaderPlatform::PackFlash()
    {
        return packFlash;
    }

    hal::SynchronousFlash& BootloaderPlatform::JournalFlash()
    {
        return journalFlash;
    }

    hal::SynchronousFlash& BootloaderPlatform::SlotFlash()
    {
        return slotFlash;
    }

    ImageTarget BootloaderPlatform::Target() const
    {
        return ImageTarget{ E_FOC_BOARD_KIND, DeviceFamily::emulated, E_FOC_DEVICE_IDENTITY };
    }

    void BootloaderPlatform::Start(uint32_t slotBegin)
    {
        hal::cortex::JumpToApplication(slotBegin);
    }

    void BootloaderPlatform::Halt()
    {
        hal::cortex::SemihostingWrite0("bootloader: halted, no intact application in the slot\n");
        for (;;)
        {
        }
    }
}
