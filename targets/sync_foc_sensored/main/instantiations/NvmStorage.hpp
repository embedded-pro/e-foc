#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include "services/util/NvmEepromRegion.hpp"
#include <cstdint>

namespace application
{
    class NvmStorage
    {
    public:
        explicit NvmStorage(hal::Eeprom& eeprom);

        services::NonVolatileMemoryImpl& Nvm();

    private:
        static constexpr uint32_t calibrationRegionOffset = 0;
        static constexpr uint32_t calibrationRegionSize = 128;
        static constexpr uint32_t configRegionOffset = calibrationRegionOffset + calibrationRegionSize;
        static constexpr uint32_t configRegionSize = 224;
        static constexpr uint32_t learnedModelRegionOffset = configRegionOffset + configRegionSize;
        static constexpr uint32_t learnedModelRegionSize = 32;
        static constexpr uint32_t faultHistoryRegionOffset = learnedModelRegionOffset + learnedModelRegionSize;
        static constexpr uint32_t faultHistoryRegionSize = 128;
        static_assert(services::CalibrationRecord::size <= calibrationRegionSize);
        static_assert(services::ConfigRecord::size <= configRegionSize);
        static_assert(services::learnedModelImageSize <= learnedModelRegionSize);
        static_assert(services::faultHistoryImageSize <= faultHistoryRegionSize);
        static_assert(faultHistoryRegionOffset == 384, "the fault history must not move");

        services::NvmEepromRegion calibrationRegion;
        services::NvmEepromRegion configRegion;
        services::NvmEepromRegion learnedModelRegion;
        services::NvmEepromRegion faultHistoryRegion;
        services::NonVolatileMemoryImpl nvm;
    };
}
