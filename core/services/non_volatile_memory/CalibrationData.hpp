#pragma once

#include <array>
#include <cstdint>

namespace services
{
    // All 4-byte fields precede the byte-sized fields to avoid implicit compiler padding.
    // 'reserved' is explicit padding to keep sizeof(CalibrationData) a multiple of 4 and
    // ensure the full struct is a deterministic, padding-free storage schema.
    struct CalibrationData
    {
        float rPhase = 0.0f;
        float lD = 0.0f;
        float lQ = 0.0f;
        float currentOffsetA = 0.0f;
        float currentOffsetB = 0.0f;
        float currentOffsetC = 0.0f;
        float inertia = 0.0f;
        float frictionCoulomb = 0.0f;
        float frictionViscous = 0.0f;
        int32_t encoderZeroOffset = 0;
        float kpCurrent = 0.0f;
        float kiCurrent = 0.0f;
        float kpVelocity = 0.0f;
        float kiVelocity = 0.0f;
        uint8_t encoderDirection = 0;
        uint8_t polePairs = 0;
        std::array<uint8_t, 2> reserved = {};
    };

    static_assert(sizeof(CalibrationData) == 60, "CalibrationData layout must be free of implicit padding");
}
