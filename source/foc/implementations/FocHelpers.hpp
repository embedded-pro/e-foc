#pragma once

#include <numbers>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "numerical/math/CompilerOptimizations.hpp"

namespace foc::detail
{
    constexpr float invSqrt3 = 0.577350269189625f;

    OPTIMIZE_FOR_SPEED
    inline float PositionWithWrapAround(float position)
    {
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr float two_pi = 2.0f * pi;

        if (position > pi)
            position -= two_pi;
        else if (position < -pi)
            position += two_pi;

        return position;
    }
}
