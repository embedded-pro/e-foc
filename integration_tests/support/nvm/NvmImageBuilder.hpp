#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include <cstdint>
#include <vector>

namespace sil
{
    // Mirrors the region map the application wires in Logic: calibration at offset 0, configuration
    // at offset 128, in a 256-byte device.
    static constexpr std::size_t nvmImageSize = 256;
    static constexpr uint32_t calibrationRegionOffset = 0;
    static constexpr uint32_t configRegionOffset = 128;

    // How a record should deviate from a well-formed one, so a scenario can describe the damage it
    // wants rather than hand-assembling bytes.
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

    // Calibration that satisfies every completeness check, including the inertia and viscous
    // friction the speed and position modes additionally require, so those modes boot to Ready.
    services::CalibrationData CompleteCalibration();

    std::vector<uint8_t> BuildNvmImage(const NvmImageSpec& spec);

    // A blank device: every byte erased, which is what the firmware sees on a first boot.
    std::vector<uint8_t> BlankNvmImage();
}
