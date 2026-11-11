#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include <array>
#include <cstdint>

namespace sil
{
    static constexpr std::size_t nvmImageSize = 512;
    static constexpr uint32_t calibrationRegionOffset = 0;
    static constexpr uint32_t configRegionOffset = 128;
    static constexpr uint32_t learnedModelRegionOffset = 352;
    static constexpr uint32_t faultHistoryRegionOffset = 384;
    static constexpr float calibratedCurrentLoopBandwidth = 2000.0f;

    using NvmImage = std::array<uint8_t, nvmImageSize>;

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

    NvmImage BuildNvmImage(const NvmImageSpec& spec);

    NvmImage BlankNvmImage();
}
