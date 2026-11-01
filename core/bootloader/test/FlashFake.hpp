#pragma once

#include "hal/synchronous_interfaces/SynchronousFlashHomogeneous.hpp"
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace bootloader
{
    class FlashFake
        : public hal::SynchronousFlashHomogeneous
    {
    public:
        static constexpr uint32_t programmingUnit = 16;

        FlashFake(uint32_t numberOfSectors, uint32_t sizeOfEachSector)
            : hal::SynchronousFlashHomogeneous(numberOfSectors, sizeOfEachSector)
            , memory(numberOfSectors * sizeOfEachSector, 0xff)
            , programmed(memory.size() / programmingUnit, false)
        {}

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address) override
        {
            ASSERT_LE(address + buffer.size(), memory.size());

            for (auto unit = address / programmingUnit; unit * programmingUnit < address + buffer.size(); ++unit)
            {
                EXPECT_FALSE(programmed[unit]) << "unit at " << unit * programmingUnit << " programmed twice without an erase";
                programmed[unit] = true;
            }

            for (std::size_t index = 0; index != buffer.size(); ++index)
                memory[address + index] &= buffer[index];
        }

        void ReadBuffer(infra::ByteRange buffer, uint32_t address) override
        {
            ASSERT_LE(address + buffer.size(), memory.size());
            std::copy(memory.begin() + address, memory.begin() + address + buffer.size(), buffer.begin());
        }

        void EraseSectors(uint32_t beginIndex, uint32_t endIndex) override
        {
            for (auto sector = beginIndex; sector != endIndex; ++sector)
            {
                const auto begin = AddressOfSector(sector);
                std::fill(memory.begin() + begin, memory.begin() + begin + SizeOfSector(sector), 0xff);
                std::fill(programmed.begin() + begin / programmingUnit, programmed.begin() + (begin + SizeOfSector(sector)) / programmingUnit, false);
                ++erasures;
            }
        }

        void Load(const std::vector<uint8_t>& contents, uint32_t address = 0)
        {
            ASSERT_LE(address + contents.size(), memory.size());
            std::copy(contents.begin(), contents.end(), memory.begin() + address);
            for (auto unit = address / programmingUnit; unit * programmingUnit < address + contents.size(); ++unit)
                programmed[unit] = true;
        }

        std::vector<uint8_t> memory;
        std::vector<bool> programmed;
        uint32_t erasures{ 0 };
    };
}
