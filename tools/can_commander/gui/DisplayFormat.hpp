#pragma once

#include <cstdint>

namespace tool
{
    constexpr int DecimalsFor(int32_t wireScale)
    {
        int decimals{ 0 };
        for (; wireScale >= 10; wireScale /= 10)
            ++decimals;
        return decimals;
    }
}
