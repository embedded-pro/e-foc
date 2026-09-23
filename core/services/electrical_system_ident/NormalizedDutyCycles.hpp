#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/DutyConversion.hpp"

namespace services::detail
{
    // Maps normalised phase voltages in [-1, 1] onto duty cycles centred on half the bus
    inline foc::PhasePwmDutyCycles NormalizedDutyCycles(foc::ThreePhase voltages)
    {
        return foc::ToDutyCycles(foc::ThreePhase{ 0.5f + 0.5f * voltages.a, 0.5f + 0.5f * voltages.b, 0.5f + 0.5f * voltages.c });
    }
}
