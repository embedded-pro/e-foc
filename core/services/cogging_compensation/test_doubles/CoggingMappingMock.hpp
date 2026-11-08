#pragma once

#include "core/services/cogging_compensation/CoggingMapping.hpp"
#include <gmock/gmock.h>

namespace services
{
    class CoggingMappingMock
        : public CoggingMapping
    {
    public:
        MOCK_METHOD(void, Measure,
            (foc::NewtonMeter torqueConstant,
                foc::NewtonMeterSecondSquared inertia,
                foc::NewtonMeterSecondPerRadian viscousFriction,
                std::size_t polePairs,
                const Config& config,
                const infra::Function<void(const CoggingMeasurement&)>& onDone),
            (override));
        MOCK_METHOD(bool, IsRunning, (), (const, override));
        MOCK_METHOD(void, Abort, (), (override));
    };
}
