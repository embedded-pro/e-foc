#pragma once

#include "bootloader/BootSequence.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousFlashInternal.hpp"
#include "infra/util/ByteRange.hpp"
#include "targets/bootloader/ImageTarget.hpp"

namespace bootloader
{
    class BootloaderPlatform
        : public ApplicationStarter
    {
    public:
        BootloaderPlatform(infra::ConstByteRange packRegion, infra::ConstByteRange journalRegion, infra::ConstByteRange slotRegion);

        hal::SynchronousFlash& PackFlash();
        hal::SynchronousFlash& JournalFlash();
        hal::SynchronousFlash& SlotFlash();
        ImageTarget Target() const;

        void Start(uint32_t slotBegin) override;
        void Halt() override;

    private:
        hal::tiva::SynchronousFlashInternal packFlash;
        hal::tiva::SynchronousFlashInternal journalFlash;
        hal::tiva::SynchronousFlashInternal slotFlash;
    };
}
