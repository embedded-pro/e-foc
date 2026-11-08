#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/FaultHistoryData.hpp"
#include "core/services/non_volatile_memory/LearnedModelData.hpp"
#include "infra/util/Function.hpp"

namespace services
{
    enum class NvmStatus
    {
        Ok,
        InvalidData,
        WriteFailed,
        HardwareFault,
        Busy
    };

    class NonVolatileMemory
    {
    protected:
        ~NonVolatileMemory() = default;

    public:
        virtual void SaveCalibration(const CalibrationData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void LoadCalibration(CalibrationData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void InvalidateCalibration(infra::Function<void(NvmStatus)> onDone) = 0;

        virtual void SaveConfig(const ConfigData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void LoadConfig(ConfigData& data, infra::Function<void(NvmStatus)> onDone) = 0;

        virtual void SaveFaultHistory(const FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void LoadFaultHistory(FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone) = 0;

        virtual void SaveLearnedModel(const LearnedModelData& model, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void LoadLearnedModel(LearnedModelData& model, infra::Function<void(NvmStatus)> onDone) = 0;
    };
}
