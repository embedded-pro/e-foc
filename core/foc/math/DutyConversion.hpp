#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace foc
{
    template<typename Modulated>
    ALWAYS_INLINE_HOT PhasePwmDutyCycles ToDutyCycles(const Modulated& output)
    {
        const auto percent = [](float normalized)
        {
            // std::clamp would pass NaN into an undefined cast; -ffinite-math-only drops this branch, so
            // the real guarantee is that configuration boundaries reject non-finite values (REQ-PERF-003)
            const auto bounded = normalized > 0.0f ? (normalized < 1.0f ? normalized : 1.0f) : 0.0f;

            return hal::Percent{ static_cast<uint8_t>(bounded * 100.0f + 0.5f) };
        };

        return PhasePwmDutyCycles{ percent(output.a), percent(output.b), percent(output.c) };
    }
}
