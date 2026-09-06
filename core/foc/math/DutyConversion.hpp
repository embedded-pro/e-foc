#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cstdint>

namespace foc
{
    template<typename Modulated>
    ALWAYS_INLINE_HOT PhasePwmDutyCycles ToDutyCycles(const Modulated& output)
    {
        const auto percent = [](float normalized)
        {
            return hal::Percent{ static_cast<uint8_t>(std::clamp(normalized, 0.0f, 1.0f) * 100.0f + 0.5f) };
        };

        return PhasePwmDutyCycles{ percent(output.a), percent(output.b), percent(output.c) };
    }
}
