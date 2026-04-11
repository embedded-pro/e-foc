#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include <gmock/gmock.h>

namespace services
{
    class NonVolatileMemoryMock
        : public NonVolatileMemory
    {
    public:
        MOCK_METHOD(void, SaveCalibration, (const CalibrationData& data, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, LoadCalibration, (CalibrationData & data, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, InvalidateCalibration, (infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, IsCalibrationValid, (infra::Function<void(bool)> onDone), (override));
        MOCK_METHOD(void, SaveConfig, (const ConfigData& data, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, LoadConfig, (ConfigData & data, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, ResetConfigToDefaults, (infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, Format, (infra::Function<void(NvmStatus)> onDone), (override));
    };
}
