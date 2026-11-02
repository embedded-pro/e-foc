#pragma once

#include "infra/util/ByteRange.hpp"
#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include <cstdint>

namespace application
{
    // A flash region held in RAM and written through to a file in the scenario folder, which outlives the emulator
    class SemihostingFileFlash
        : public services::SynchronousFlashMemory
    {
    public:
        SemihostingFileFlash(const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex) override;

    private:
        void Load();
        void Store() const;

        const char* path;
        infra::ByteRange storage;
    };
}
