#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace services
{
    struct CoggingMap
    {
        std::array<int8_t, coggingMapBins> torque{};
        float torqueScale{ 0.0f };
        foc::NewtonMeter peak{ 0.0f };
        float agreement{ 0.0f };
    };

    struct CoggingMeasurement
    {
        std::optional<CoggingMap> map;
        foc::NewtonMeter frictionCoulomb{ 0.0f };
    };

    foc::NewtonMeter CoggingPeak(const std::array<int8_t, coggingMapBins>& torque, float torqueScale);
}
