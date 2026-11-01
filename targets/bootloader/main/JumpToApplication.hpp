#pragma once

#include DEVICE_HEADER
#include <cstdint>

namespace bootloader
{
    [[noreturn]] inline void JumpToApplication(uint32_t slotBegin)
    {
        const auto* vectors = reinterpret_cast<const uint32_t*>(slotBegin);
        const auto stackPointer = vectors[0];
        const auto resetHandler = vectors[1];

        SCB->VTOR = slotBegin;
        __DSB();
        __ISB();

        // One asm block: once the stack pointer moves, nothing may be read back from the old stack
        asm volatile(
            "msr msp, %0\n"
            "bx %1\n"
            :
            : "r"(stackPointer), "r"(resetHandler)
            : "memory");
        __builtin_unreachable();
    }
}
