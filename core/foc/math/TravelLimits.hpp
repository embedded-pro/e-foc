#pragma once

#include "e-foc-protocol/foc/Units.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>

namespace foc
{
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

            const auto folded = math::WrapToPi(setpoint.Value());
            return folded >= minimum.Value() && folded <= maximum.Value();
        }

        OPTIMIZE_FOR_SPEED Radians Hold(Radians reference) const
        {
            return Radians{ std::clamp(math::WrapToPi(reference.Value()), minimum.Value(), maximum.Value()) };
        }
    };
}
