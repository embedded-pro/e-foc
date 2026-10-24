#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace tool
{
    constexpr int DecimalsFor(int32_t wireScale)
    {
        int decimals{ 0 };
        for (; wireScale >= 10; wireScale /= 10)
            ++decimals;
        return decimals;
    }

    constexpr double SetpointLimit(float firmwareLimit, int32_t wireScale)
    {
        return std::min(static_cast<double>(firmwareLimit), static_cast<double>(std::numeric_limits<int16_t>::max()) / wireScale);
    }
}
