#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/services/non_volatile_memory/NvmRecord.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/NvmImage.hpp"
#include "services/util/NvmRegion.hpp"
#include <cstddef>

namespace services
{
    class NonVolatileMemoryImpl
        : public NonVolatileMemory
    {
    public:
        NonVolatileMemoryImpl(NvmRegion& calibrationRegion, NvmRegion& configRegion, NvmRegion& faultHistoryRegion, NvmRegion& learnedModelRegion);

        void SaveCalibration(const CalibrationData& data, infra::Function<void(NvmStatus)> onDone) override;
        void LoadCalibration(CalibrationData& data, infra::Function<void(NvmStatus)> onDone) override;
        void InvalidateCalibration(infra::Function<void(NvmStatus)> onDone) override;

        void SaveConfig(const ConfigData& data, infra::Function<void(NvmStatus)> onDone) override;
        void LoadConfig(ConfigData& data, infra::Function<void(NvmStatus)> onDone) override;

        void SaveFaultHistory(const FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone) override;
        void LoadFaultHistory(FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone) override;

        void SaveLearnedModel(const LearnedModelData& model, infra::Function<void(NvmStatus)> onDone) override;
        void LoadLearnedModel(LearnedModelData& model, infra::Function<void(NvmStatus)> onDone) override;

    private:
        template<auto Serialize, std::size_t Size, class Data>
        void Store(NvmImage<Size>& image, const Data& data, const infra::Function<void(NvmStatus)>& onDone);
        template<auto Deserialize, std::size_t Size, class Data>
        void Fetch(NvmImage<Size>& image, Data& data, NvmStatus whenInvalid, const infra::Function<void(NvmStatus)>& onDone);

        bool Admit(const infra::Function<void(NvmStatus)>& onDone);
        void Complete(NvmStatus status);

        NvmImage<CalibrationRecord::size> calibration;
        NvmImage<ConfigRecord::size> config;
        NvmImage<faultHistoryImageSize> faultHistory;
        NvmImage<learnedModelImageSize> learnedModel;

        infra::AutoResetFunction<void(NvmStatus)> onDone;
        NvmStatus whenInvalid{ NvmStatus::InvalidData };
        bool busy{ false };
    };
}
