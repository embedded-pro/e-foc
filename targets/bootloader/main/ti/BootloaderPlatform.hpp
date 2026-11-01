#pragma once

#include "core/bootloader/BootSequence.hpp"
#include "core/bootloader/ImageFormat.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousFlashInternal.hpp"
#include "infra/util/ByteRange.hpp"

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
        TargetIdentity Identity() const;

        void Start(uint32_t slotBegin) override;
        void Halt() override;

    private:
        hal::tiva::SynchronousFlashInternal packFlash;
        hal::tiva::SynchronousFlashInternal journalFlash;
        hal::tiva::SynchronousFlashInternal slotFlash;
    };
}
