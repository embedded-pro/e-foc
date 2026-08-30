#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <algorithm>

namespace services::detail
{
    inline foc::PhasePwmDutyCycles NormalizedDutyCycles(foc::ThreePhase voltages)
    {
        constexpr float offset = 50.0f;
        constexpr float scale = 50.0f;
        return foc::PhasePwmDutyCycles{
            hal::Percent{ static_cast<uint8_t>(std::clamp(offset + voltages.a * scale, 0.0f, 100.0f)) },
            hal::Percent{ static_cast<uint8_t>(std::clamp(offset + voltages.b * scale, 0.0f, 100.0f)) },
            hal::Percent{ static_cast<uint8_t>(std::clamp(offset + voltages.c * scale, 0.0f, 100.0f)) }
        };
    }
}
