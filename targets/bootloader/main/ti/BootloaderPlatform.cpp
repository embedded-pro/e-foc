#include "BootloaderPlatform.hpp"
#include DEVICE_HEADER
#include "hal/cortex_m/JumpToApplication.hpp"

extern "C"
{
    void HardwareInitialization()
    {}

    void Default_Handler_Forwarded()
    {
        for (;;)
        {
        }
    }
}

namespace bootloader
{
    namespace
    {
        constexpr uint32_t deviceClassShift = 16;
        constexpr uint32_t deviceClassMask = 0xff;
    }

    BootloaderPlatform::BootloaderPlatform(infra::ConstByteRange packRegion, infra::ConstByteRange journalRegion, infra::ConstByteRange slotRegion)
        : packFlash(packRegion)
        , journalFlash(journalRegion)
        , slotFlash(slotRegion)
    {}

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
        return ImageTarget{ E_FOC_BOARD_KIND, DeviceFamily::tivaTm4c, (SYSCTL->DID0 >> deviceClassShift) & deviceClassMask };
    }

    void BootloaderPlatform::Start(uint32_t slotBegin)
    {
        hal::cortex::JumpToApplication(slotBegin);
    }

    void BootloaderPlatform::Halt()
    {
        for (;;)
        {
        }
    }
}
