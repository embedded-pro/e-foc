#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/DutyConversion.hpp"

namespace services::detail
{
    inline foc::PhasePwmDutyCycles NormalizedDutyCycles(foc::ThreePhase voltages)
    {
        return foc::ToDutyCycles(foc::ThreePhase{ 0.5f + 0.5f * voltages.a, 0.5f + 0.5f * voltages.b, 0.5f + 0.5f * voltages.c });
    }
}
