#pragma once

#include "hal/interfaces/Flash.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "source/services/NonVolatileMemory/NonVolatileMemory.hpp"
#include <array>
#include <cstdint>

namespace services
{
    static constexpr uint32_t CalibrationMagic = 0xCAFEF00D;
    static constexpr uint32_t ConfigMagic = 0xDEADBEEF;
    static constexpr uint8_t CalibrationLayoutVersion = 1;
    static constexpr uint8_t ConfigLayoutVersion = 1;

#pragma pack(push, 1)

    struct CalibrationStorageRecord
    {
        uint32_t magic;
        uint8_t version;
        uint32_t crc32;
        CalibrationData data;
    };

    struct ConfigStorageRecord
    {
        uint32_t magic;
        uint8_t version;
        uint32_t crc32;
        ConfigData data;
    };

#pragma pack(pop)

    class NonVolatileMemoryImpl
        : public NonVolatileMemory
    {
    public:
        explicit NonVolatileMemoryImpl(hal::Flash& flash);

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

        void OnConfigErased();
        void OnConfigWritten();
        void OnConfigReadBack();

        void OnCalibrationSectorFormattedDuringFormat();

        static uint32_t ComputeCrc(infra::ConstByteRange bytes);

        static constexpr uint32_t calibrationSectorIndex = 0;
        static constexpr uint32_t configSectorIndex = 1;

        hal::Flash& flash;

        std::array<uint8_t, sizeof(CalibrationStorageRecord)> calibrationBuffer{};
        std::array<uint8_t, sizeof(CalibrationStorageRecord)> calibrationReadBackBuffer{};
        std::array<uint8_t, sizeof(ConfigStorageRecord)> configBuffer{};
        std::array<uint8_t, sizeof(ConfigStorageRecord)> configReadBackBuffer{};

        infra::AutoResetFunction<void(NvmStatus)> onCalibrationDone;
        infra::AutoResetFunction<void(NvmStatus)> onConfigDone;
        infra::AutoResetFunction<void(NvmStatus)> onFormatDone;
        infra::AutoResetFunction<void(bool)> onIsCalibrationValidDone;

        CalibrationData* pendingCalibrationOutput = nullptr;
        ConfigData* pendingConfigOutput = nullptr;
    };
}
