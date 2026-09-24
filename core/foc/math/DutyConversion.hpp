#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace foc
{
    inline float DutyFraction(hal::DutyCycle duty)
    {
        return static_cast<float>(duty.Value()) * (1.0f / static_cast<float>(hal::DutyCycle::fullScale));
    }

    template<typename Modulated>
    ALWAYS_INLINE_HOT PhasePwmDutyCycles ToDutyCycles(const Modulated& output)
    {
        const auto toDutyCycle = [](float normalized)
        {
            const auto bounded = normalized > 0.0f ? (normalized < 1.0f ? normalized : 1.0f) : 0.0f;

            return hal::DutyCycle{ static_cast<uint32_t>(bounded * static_cast<float>(hal::DutyCycle::fullScale) + 0.5f) };
        };

        return PhasePwmDutyCycles{ toDutyCycle(output.a), toDutyCycle(output.b), toDutyCycle(output.c) };
    }
}
