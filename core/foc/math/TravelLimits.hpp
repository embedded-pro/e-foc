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
        // Circular mode: positions are angles within the turn; linear mode: multi-turn positions.
        bool circular{ false };

        bool IsOn() const
        {
            return minimum.Value() < maximum.Value();
        }

        bool Folds() const
        {
            return circular && !IsOn();
        }

        bool Admits(Radians setpoint) const
        {
            if (!IsOn())
                return true;

            const auto position = InFrame(setpoint.Value());
            return position >= minimum.Value() && position <= maximum.Value();
        }

        OPTIMIZE_FOR_SPEED Radians Hold(Radians reference) const
        {
            return Radians{ std::clamp(InFrame(reference.Value()), minimum.Value(), maximum.Value()) };
        }

    private:
        OPTIMIZE_FOR_SPEED float InFrame(float position) const
        {
            return circular ? math::WrapToPi(position) : position;
        }
    };
}
