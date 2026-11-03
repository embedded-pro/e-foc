#pragma once

#include "core/platform_abstraction/ActiveInterrupt.hpp"
#include <cstdint>

namespace application
{
    class ControlInterrupt
    {
    public:
        // Called from the control interrupt: true when it was requested again before returning.
        static bool PendsAgain()
        {
            return ActiveInterruptPendsAgain(*reinterpret_cast<volatile uint32_t*>(scbIcsr), reinterpret_cast<volatile uint32_t*>(nvicIspr));
        }

    private:
        static constexpr uintptr_t scbIcsr = 0xE000ED04u;
        static constexpr uintptr_t nvicIspr = 0xE000E200u;
    };
}
