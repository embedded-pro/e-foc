#pragma once

#include "bootloader/BootSequence.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include "targets/platform_implementations/cortex_m_common/ImageTarget.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingFileFlash.hpp"

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
        application::SemihostingFileFlash packFlash;
        application::SemihostingFileFlash journalFlash;
        services::SynchronousFlashMemory slotFlash;
    };
}
