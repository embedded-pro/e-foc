#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <cmath>

namespace services
{
    inline bool ExceedsInjectionLimit(const foc::PhaseCurrents& currents, foc::Ampere limit)
    {
        const auto maximum = limit.Value();

        return std::fabs(currents.a.Value()) > maximum ||
               std::fabs(currents.b.Value()) > maximum ||
               std::fabs(currents.c.Value()) > maximum;
    }
}
