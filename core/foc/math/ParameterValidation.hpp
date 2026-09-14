#pragma once

#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/FiniteGuard.hpp"

namespace foc
{
    inline bool IsWithinInclusive(float value, float minValue, float maxValue)
    {
        return IsFiniteValue(value) && value >= minValue && value <= maxValue;
    }

    inline bool IsFinitePositive(float value)
    {
        return IsFiniteValue(value) && value > 0.0f;
    }

    inline bool IsFiniteNonNegative(float value)
    {
        return IsFiniteValue(value) && value >= 0.0f;
    }

    inline bool IsAcceptableCurrentBandwidth(float bandwidth)
    {
        return IsWithinInclusive(bandwidth, CommandLimits::minBandwidth, CommandLimits::maxCurrentBandwidth);
    }

    inline bool IsAcceptableSpeedBandwidth(float bandwidth)
    {
        return IsWithinInclusive(bandwidth, CommandLimits::minBandwidth, CommandLimits::maxSpeedBandwidth);
    }

    inline bool IsAcceptablePositionBandwidth(float bandwidth)
    {
        return IsWithinInclusive(bandwidth, CommandLimits::minBandwidth, CommandLimits::maxPositionBandwidth);
    }
}
