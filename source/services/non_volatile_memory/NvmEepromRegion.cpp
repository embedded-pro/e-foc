#include "source/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    NvmEepromRegion::NvmEepromRegion(hal::Eeprom& eeprom, uint32_t baseAddress, uint32_t regionSize)
        : eeprom(eeprom)
        , baseAddress(baseAddress)
        , regionSize(regionSize)
        , eraseBuffer{}
    {
        really_assert(regionSize <= maxRegionSize);
        really_assert(regionSize <= eeprom.Size());
        really_assert(baseAddress <= eeprom.Size() - regionSize);
        eraseBuffer.fill(0xFF);
    }

    void NvmEepromRegion::Write(infra::ConstByteRange data, infra::Function<void()> onDone)
    {
        really_assert(data.size() <= regionSize);
        eeprom.WriteBuffer(data, baseAddress, onDone);
    }

    void NvmEepromRegion::Read(infra::ByteRange data, infra::Function<void()> onDone)
    {
        really_assert(data.size() <= regionSize);
        eeprom.ReadBuffer(data, baseAddress, onDone);
    }

    void NvmEepromRegion::Erase(infra::Function<void()> onDone)
    {
        eeprom.WriteBuffer(infra::ConstByteRange{ eraseBuffer.data(), eraseBuffer.data() + regionSize }, baseAddress, onDone);
    }

    std::size_t NvmEepromRegion::Size() const
    {
        return regionSize;
    }
}
