#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/services/non_volatile_memory/NvmRegion.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <array>
#include <cstdint>

namespace services
{
    // Bumping a layout version invalidates all stored data of that type; see service-nvm.md.
    static constexpr uint32_t CalibrationMagic = 0xCAFEF00D;
    static constexpr uint32_t ConfigMagic = 0xDEADBEEF;
    static constexpr uint8_t CalibrationLayoutVersion = 4;
    static constexpr uint8_t ConfigLayoutVersion = 13;

    class NonVolatileMemoryImpl
        : public NonVolatileMemory
    {
    public:
        static constexpr std::size_t calibrationRecordSize =
            sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(CalibrationData);
        static constexpr std::size_t configRecordSize =
            sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(ConfigData);

        NonVolatileMemoryImpl(NvmRegion& calibrationRegion, NvmRegion& configRegion, NvmRegion& faultHistoryRegion, NvmRegion& learnedModelRegion);

        void SaveCalibration(const CalibrationData& data, infra::Function<void(NvmStatus)> onDone) override;
        void LoadCalibration(CalibrationData& data, infra::Function<void(NvmStatus)> onDone) override;
        void InvalidateCalibration(infra::Function<void(NvmStatus)> onDone) override;
        void IsCalibrationValid(infra::Function<void(bool)> onDone) override;

        void SaveConfig(const ConfigData& data, infra::Function<void(NvmStatus)> onDone) override;
        void LoadConfig(ConfigData& data, infra::Function<void(NvmStatus)> onDone) override;
        void ResetConfigToDefaults(infra::Function<void(NvmStatus)> onDone) override;

        void Format(infra::Function<void(NvmStatus)> onDone) override;

        void SaveFaultHistory(const FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone) override;
        void LoadFaultHistory(FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone) override;

        void SaveLearnedModel(const LearnedModelData& model, infra::Function<void(NvmStatus)> onDone) override;
        void LoadLearnedModel(LearnedModelData& model, infra::Function<void(NvmStatus)> onDone) override;

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

        void OnCalibrationInvalidationVerified();
        void OnCalibrationSectorFormattedDuringFormat();
        void OnCalibrationFormatVerified();
        void OnConfigSectorFormattedDuringFormat();

        void CompleteCalibration(NvmStatus status);
        void CompleteConfig(NvmStatus status);
        void CompleteFormat(NvmStatus status);
        void CompleteIsCalibrationValid(bool valid);
        void CompleteFaultHistory(NvmStatus status);
        void OnFaultHistoryWritten();
        void OnFaultHistoryReadBack();
        void OnFaultHistoryReadForLoad();
        void CompleteLearnedModel(NvmStatus status);
        void OnLearnedModelWritten();
        void OnLearnedModelReadBack();
        void OnLearnedModelReadForLoad();

        static uint32_t ComputeCrc(infra::ConstByteRange bytes);

        NvmRegion& calibrationRegion;
        NvmRegion& configRegion;
        NvmRegion& faultHistoryRegion;
        NvmRegion& learnedModelRegion;

        std::array<uint8_t, calibrationRecordSize> calibrationBuffer{};
        std::array<uint8_t, calibrationRecordSize> calibrationReadBackBuffer{};
        std::array<uint8_t, configRecordSize> configBuffer{};
        std::array<uint8_t, configRecordSize> configReadBackBuffer{};
        std::array<uint8_t, faultHistoryImageSize> faultHistoryBuffer{};
        std::array<uint8_t, faultHistoryImageSize> faultHistoryReadBackBuffer{};
        std::array<uint8_t, learnedModelImageSize> learnedModelBuffer{};
        std::array<uint8_t, learnedModelImageSize> learnedModelReadBackBuffer{};

        infra::AutoResetFunction<void(NvmStatus)> onCalibrationDone;
        infra::AutoResetFunction<void(NvmStatus)> onConfigDone;
        infra::AutoResetFunction<void(NvmStatus)> onFormatDone;
        infra::AutoResetFunction<void(bool)> onIsCalibrationValidDone;
        infra::AutoResetFunction<void(NvmStatus)> onFaultHistoryDone;
        infra::AutoResetFunction<void(NvmStatus)> onLearnedModelDone;

        CalibrationData* pendingCalibrationOutput = nullptr;
        ConfigData* pendingConfigOutput = nullptr;
        FaultHistoryData* pendingFaultHistoryOutput = nullptr;
        LearnedModelData* pendingLearnedModelOutput = nullptr;

        bool busy{ false };
    };
}
