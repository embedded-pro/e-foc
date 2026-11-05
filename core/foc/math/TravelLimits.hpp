#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>

namespace foc
{
    // Single-turn mechanical angles from the alignment zero, in [-pi, pi).
    struct TravelLimits
    {
        Radians minimum{ 0.0f };
        Radians maximum{ 0.0f };

        bool IsOn() const
        {
            return minimum.Value() < maximum.Value();
        }

        bool Admits(Radians setpoint) const
        {
            if (!IsOn())
                return true;

            const auto folded = detail::PositionWithWrapAround(setpoint.Value());
            return folded >= minimum.Value() && folded <= maximum.Value();
        }

        // Only while IsOn(): with the minimum above the maximum the clamp has no meaning.
        OPTIMIZE_FOR_SPEED Radians Hold(Radians reference) const
        {
            return Radians{ std::clamp(detail::PositionWithWrapAround(reference.Value()), minimum.Value(), maximum.Value()) };
        }
    };
}
