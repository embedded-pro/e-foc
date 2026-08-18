#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cmath>
#include <numbers>

namespace foc::detail
{
    OPTIMIZE_FOR_SPEED
    inline float PositionWithWrapAround(float position)
    {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        return position - two_pi * std::round(position / two_pi);
    }
}
