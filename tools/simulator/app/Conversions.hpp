#pragma once

#include <numbers>

namespace simulator::conversions
{
    inline constexpr float rpmToRadPerSec = std::numbers::pi_v<float> / 30.0f;
    inline constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0f;
}
