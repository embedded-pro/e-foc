#pragma once

#include "infra/util/AutoResetFunction.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/services/non_volatile_memory/NvmRegion.hpp"
#include <array>
#include <cstdint>

namespace services
{
    // On-media record layout constants. Bumping a version constant invalidates all stored
    // data of that type, protecting against silent corruption after a firmware update.
    static constexpr uint32_t CalibrationMagic = 0xCAFEF00D;
    static constexpr uint32_t ConfigMagic = 0xDEADBEEF;
    static constexpr uint8_t CalibrationLayoutVersion = 1;
    static constexpr uint8_t ConfigLayoutVersion = 1;

    class NonVolatileMemoryImpl
        : public NonVolatileMemory
    {
    public:
        // Record layout: [magic:4][version:1][crc32:4][data:N]
        static constexpr std::size_t calibrationRecordSize =
            sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(CalibrationData);
        static constexpr std::size_t configRecordSize =
            sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(ConfigData);

        NonVolatileMemoryImpl(NvmRegion& calibrationRegion, NvmRegion& configRegion);

        void SaveCalibration(const CalibrationData& data, infra::Function<void(NvmStatus)> onDone) override;
        void LoadCalibration(CalibrationData& data, infra::Function<void(NvmStatus)> onDone) override;
        void InvalidateCalibration(infra::Function<void(NvmStatus)> onDone) override;
        void IsCalibrationValid(infra::Function<void(bool)> onDone) override;

        void SaveConfig(const ConfigData& data, infra::Function<void(NvmStatus)> onDone) override;
        void LoadConfig(ConfigData& data, infra::Function<void(NvmStatus)> onDone) override;
        void ResetConfigToDefaults(infra::Function<void(NvmStatus)> onDone) override;

        void Format(infra::Function<void(NvmStatus)> onDone) override;

    private:
        void OnCalibrationErased();
        void OnCalibrationWritten();
        void OnCalibrationReadBack();
        void OnCalibrationReadForLoad();
        void OnCalibrationReadForIsValid();

        void OnConfigErased();
        void OnConfigWritten();
        void OnConfigReadBack();
        void OnConfigReadForLoad();

        void OnCalibrationSectorFormattedDuringFormat();

        static uint32_t ComputeCrc(infra::ConstByteRange bytes);

        NvmRegion& calibrationRegion;
        NvmRegion& configRegion;

        std::array<uint8_t, calibrationRecordSize> calibrationBuffer{};
        std::array<uint8_t, calibrationRecordSize> calibrationReadBackBuffer{};
        std::array<uint8_t, configRecordSize> configBuffer{};
        std::array<uint8_t, configRecordSize> configReadBackBuffer{};

        infra::AutoResetFunction<void(NvmStatus)> onCalibrationDone;
        infra::AutoResetFunction<void(NvmStatus)> onConfigDone;
        infra::AutoResetFunction<void(NvmStatus)> onFormatDone;
        infra::AutoResetFunction<void(bool)> onIsCalibrationValidDone;

        CalibrationData* pendingCalibrationOutput = nullptr;
        ConfigData* pendingConfigOutput = nullptr;
    };
}
