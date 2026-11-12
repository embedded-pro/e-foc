#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cstdint>

namespace foc
{
    struct AngleRotation
    {
        float cosine;
        float sine;
    };

    // Largest advance the series below are held to: cosine error δ⁴/24 = 6.3e-4, sine error δ⁵/120 = 4.4e-5
    constexpr float maxDelayAdvance{ 0.35f };

    constexpr float DutyDelay(uint32_t dutyUpdateDelayPeriods, float samplingFrequency)
    {
        return samplingFrequency > 0.0f ? (static_cast<float>(dutyUpdateDelayPeriods) + 0.5f) / samplingFrequency : 0.0f;
    }

    // Rotates (cosθ, sinθ) through ω·delay; the speed is electrical, the delay in seconds
    ALWAYS_INLINE_HOT AngleRotation AdvanceRotation(float cosTheta, float sinTheta, float electricalSpeed, float delay)
    {
        const float advance = std::clamp(electricalSpeed * delay, -maxDelayAdvance, maxDelayAdvance);
        const float squared = advance * advance;
        const float cosAdvance = 1.0f - 0.5f * squared;
        const float sinAdvance = advance * (1.0f - squared * (1.0f / 6.0f));

        return { cosTheta * cosAdvance - sinTheta * sinAdvance, sinTheta * cosAdvance + cosTheta * sinAdvance };
    }
}
