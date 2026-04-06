#pragma once

#include <cstdint>

namespace services
{
    struct CalibrationData
    {
        float rPhase;
        float lD;
        float lQ;
        float currentOffsetA;
        float currentOffsetB;
        float currentOffsetC;
        float inertia;
        float frictionCoulomb;
        float frictionViscous;
        int32_t encoderZeroOffset;
        uint8_t encoderDirection;
        uint8_t polePairs;
        float kpCurrent;
        float kiCurrent;
        float kpVelocity;
        float kiVelocity;
    };
}
