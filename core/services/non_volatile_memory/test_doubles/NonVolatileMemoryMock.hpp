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
        MOCK_METHOD(void, SaveConfig, (const ConfigData& data, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, LoadConfig, (ConfigData & data, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, SaveFaultHistory, (const FaultHistoryData& history, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, LoadFaultHistory, (FaultHistoryData & history, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, SaveLearnedModel, (const LearnedModelData& model, infra::Function<void(NvmStatus)> onDone), (override));
        MOCK_METHOD(void, LoadLearnedModel, (LearnedModelData & model, infra::Function<void(NvmStatus)> onDone), (override));
    };
}
