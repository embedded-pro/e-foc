#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    template<typename Modulated>
    ALWAYS_INLINE_HOT PhasePwmDutyCycles ToDutyCycles(const Modulated& output)
    {
        const auto percent = [](float normalized)
        {
            // std::clamp would pass NaN on to the PWM; -ffinite-math-only drops this branch, so the real
            // guarantee is that configuration boundaries reject non-finite values (REQ-PERF-003)
            const auto bounded = normalized > 0.0f ? (normalized < 1.0f ? normalized : 1.0f) : 0.0f;

            return hal::FractionalPercent{ bounded * 100.0f };
        };

        return PhasePwmDutyCycles{ percent(output.a), percent(output.b), percent(output.c) };
    }
}
