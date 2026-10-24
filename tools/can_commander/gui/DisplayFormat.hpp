#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace tool
{
    // Places after the point a fixed-point wire value resolves: a scale of 100 carries 0.01.
    constexpr int DecimalsFor(int32_t wireScale)
    {
        int decimals{ 0 };
        for (; wireScale >= 10; wireScale /= 10)
            ++decimals;
        return decimals;
    }

    // Largest magnitude both the firmware accepts and a 16-bit wire field can carry.
    constexpr double SetpointLimit(float firmwareLimit, int32_t wireScale)
    {
        return std::min(static_cast<double>(firmwareLimit), static_cast<double>(std::numeric_limits<int16_t>::max()) / wireScale);
    }
}
