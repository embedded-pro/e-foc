#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "numerical/math/Math.hpp"
#include <cmath>

namespace services
{
    inline bool ExceedsInjectionLimit(const foc::PhaseCurrents& currents, foc::Ampere limit)
    {
        const auto maximum = limit.Value();

        return math::Abs(currents.a.Value()) > maximum ||
               math::Abs(currents.b.Value()) > maximum ||
               math::Abs(currents.c.Value()) > maximum;
    }
}
