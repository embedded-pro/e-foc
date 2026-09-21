#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include <cstdint>
#include <vector>

namespace sil
{
    static constexpr std::size_t nvmImageSize = 256;
    static constexpr uint32_t calibrationRegionOffset = 0;
    static constexpr uint32_t configRegionOffset = 128;

    enum class RecordDamage : uint8_t
    {
        none,
        badMagic,
        staleVersion,
        corruptCrc
    };

    struct NvmImageSpec
    {
        bool includeCalibration{ true };
        bool includeConfig{ true };
        RecordDamage calibrationDamage{ RecordDamage::none };
        RecordDamage configDamage{ RecordDamage::none };
        services::CalibrationData calibration{};
        services::ConfigData config{};
    };

    services::CalibrationData CompleteCalibration();

    std::vector<uint8_t> BuildNvmImage(const NvmImageSpec& spec);

    std::vector<uint8_t> BlankNvmImage();
}
