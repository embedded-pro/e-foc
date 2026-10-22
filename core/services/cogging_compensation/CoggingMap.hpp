#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include <array>
#include <cstdint>

namespace services
{
    struct CoggingMap
    {
        std::array<int8_t, coggingMapBins> torque{};
        float torqueScale{ 0.0f };
        foc::NewtonMeter peak{ 0.0f };
    };

    foc::NewtonMeter CoggingPeak(const std::array<int8_t, coggingMapBins>& torque, float torqueScale);
}
