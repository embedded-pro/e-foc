#pragma once

#include "hal/interfaces/FlashHomogeneous.hpp"
#include "infra/util/ByteRange.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingFileFlash.hpp"
#include <cstdint>

namespace application
{
    class SemihostingAsyncFlash
        : public hal::FlashHomogeneous
    {
    public:
        SemihostingAsyncFlash(const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone) override;

    private:
        SemihostingFileFlash file;
    };
}
