#include "targets/sync_foc_sensored/main/instantiations/NvmStorage.hpp"

namespace application
{
    NvmStorage::NvmStorage(hal::Eeprom& eeprom)
        : calibrationRegion{ eeprom, calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ eeprom, configRegionOffset, configRegionSize }
        , learnedModelRegion{ eeprom, learnedModelRegionOffset, learnedModelRegionSize }
        , faultHistoryRegion{ eeprom, faultHistoryRegionOffset, faultHistoryRegionSize }
        , nvm{ calibrationRegion, configRegion, faultHistoryRegion, learnedModelRegion }
    {}

    services::NonVolatileMemoryImpl& NvmStorage::Nvm()
    {
        return nvm;
    }
}
