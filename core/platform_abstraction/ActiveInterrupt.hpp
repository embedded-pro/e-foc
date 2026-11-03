#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace application
{
    // Cortex-M: ICSR.VECTACTIVE holds the exception number, and external interrupt n is exception n + 16.
    ALWAYS_INLINE_HOT bool ActiveInterruptPendsAgain(uint32_t icsr, const volatile uint32_t* setPending)
    {
        constexpr uint32_t vectActiveMask{ 0x1ffu };
        constexpr uint32_t firstExternalException{ 16 };

        const auto exception = icsr & vectActiveMask;
        if (exception < firstExternalException)
            return false;

        const auto irq = exception - firstExternalException;
        return (setPending[irq / 32] & (1u << (irq % 32))) != 0;
    }
}
