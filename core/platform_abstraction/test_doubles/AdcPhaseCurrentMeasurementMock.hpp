#pragma once

#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include <gmock/gmock.h>

namespace application
{
    class AdcPhaseCurrentMeasurementMock
        : public AdcPhaseCurrentMeasurement
    {
    public:
        MOCK_METHOD(void, Measure,
            (const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone),
            (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}
