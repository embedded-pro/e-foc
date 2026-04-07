#pragma once

#include "hal/interfaces/Flash.hpp"
#include "source/services/NonVolatileMemory/NvmRegion.hpp"

namespace services
{
    class NvmFlashRegion
        : public NvmRegion
    {
    public:
        NvmFlashRegion(hal::Flash& flash, uint32_t sectorIndex);

        void Write(infra::ConstByteRange data, infra::Function<void()> onDone) override;
        void Read(infra::ByteRange data, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;
        std::size_t Size() const override;

    private:
        hal::Flash& flash;
        uint32_t sectorIndex;
    };
}
