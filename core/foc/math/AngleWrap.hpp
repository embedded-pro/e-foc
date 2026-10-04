#pragma once

#include <cmath>
#include <numbers>

namespace foc::detail
{
    inline float PositionWithWrapAround(float position)
    {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        return position - two_pi * std::round(position / two_pi);
    }
}
