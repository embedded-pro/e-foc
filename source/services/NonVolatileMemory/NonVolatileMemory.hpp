#pragma once

#include "infra/util/Function.hpp"
#include "source/services/NonVolatileMemory/CalibrationData.hpp"
#include "source/services/NonVolatileMemory/ConfigData.hpp"

namespace services
{
    enum class NvmStatus
    {
        Ok,
        InvalidData,
        VersionMismatch,
        WriteFailed,
        HardwareFault
    };

    class NonVolatileMemory
    {
    public:
        virtual ~NonVolatileMemory() = default;

        virtual void SaveCalibration(const CalibrationData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void LoadCalibration(CalibrationData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void InvalidateCalibration(infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void IsCalibrationValid(infra::Function<void(bool)> onDone) = 0;

        virtual void SaveConfig(const ConfigData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void LoadConfig(ConfigData& data, infra::Function<void(NvmStatus)> onDone) = 0;
        virtual void ResetConfigToDefaults(infra::Function<void(NvmStatus)> onDone) = 0;

        virtual void Format(infra::Function<void(NvmStatus)> onDone) = 0;
    };
}
