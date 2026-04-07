#include "source/services/NonVolatileMemory/NvmFlashRegion.hpp"

namespace services
{
    NvmFlashRegion::NvmFlashRegion(hal::Flash& flash, uint32_t sectorIndex)
        : flash(flash)
        , sectorIndex(sectorIndex)
    {
    }

    void NvmFlashRegion::Write(infra::ConstByteRange data, infra::Function<void()> onDone)
    {
        flash.WriteBuffer(data, flash.AddressOfSector(sectorIndex), std::move(onDone));
    }

    void NvmFlashRegion::Read(infra::ByteRange data, infra::Function<void()> onDone)
    {
        flash.ReadBuffer(data, flash.AddressOfSector(sectorIndex), std::move(onDone));
    }

    void NvmFlashRegion::Erase(infra::Function<void()> onDone)
    {
        flash.EraseSectors(sectorIndex, sectorIndex + 1, std::move(onDone));
    }

    std::size_t NvmFlashRegion::Size() const
    {
        return flash.SizeOfSector(sectorIndex);
    }
}
