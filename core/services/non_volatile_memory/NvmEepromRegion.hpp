#pragma once

#include "core/services/non_volatile_memory/NvmRegion.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class NvmEepromRegion
        : public NvmRegion
    {
    public:
        static constexpr std::size_t maxRegionSize = 256;

        NvmEepromRegion(hal::Eeprom& eeprom, uint32_t baseAddress, uint32_t regionSize);

        void Write(infra::ConstByteRange data, infra::Function<void()> onDone) override;
        void Read(infra::ByteRange data, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;
        std::size_t Size() const override;

    private:
        hal::Eeprom& eeprom;
        uint32_t baseAddress;
        uint32_t regionSize;
        std::array<uint8_t, maxRegionSize> eraseBuffer{};
    };
}
