#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cstdint>

namespace foc
{
    // A low-side shunt reads its phase only while that leg's low-side switch conducts around the PWM centre, so a
    // duty above the board's limit hides the phase from the current loop. Lowering all three duties together keeps
    // the line-to-line voltages, and with them the vector the current loop asked for; only a vector wider than the
    // limit is scaled down, keeping its direction.
    ALWAYS_INLINE_HOT PhasePwmDutyCycles KeepLowSideShuntsReadable(const PhasePwmDutyCycles& duties, hal::DutyCycle maxReadable)
    {
        const auto limit = maxReadable.Value();
        const auto highest = std::max({ duties.a.Value(), duties.b.Value(), duties.c.Value() });

        if (highest <= limit)
            return duties;

        const auto lowest = std::min({ duties.a.Value(), duties.b.Value(), duties.c.Value() });
        const auto shift = std::min(highest - limit, lowest);
        const auto shiftedHighest = highest - shift;
        const auto scale = shiftedHighest > limit ? static_cast<float>(limit) / static_cast<float>(shiftedHighest) : 1.0f;

        const auto keep = [shift, scale](hal::DutyCycle duty)
        {
            return hal::DutyCycle{ static_cast<uint32_t>(static_cast<float>(duty.Value() - shift) * scale + 0.5f) };
        };

        return PhasePwmDutyCycles{ keep(duties.a), keep(duties.b), keep(duties.c) };
    }
}
