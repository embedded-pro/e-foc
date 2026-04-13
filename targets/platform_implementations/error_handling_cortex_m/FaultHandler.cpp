#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include DEVICE_HEADER
#include <cstdlib>

namespace application
{
    __attribute__((section(".error_handling"), used)) PersistentFaultData persistentFaultData;
}

extern "C"
{
    extern uint32_t _text;
    extern uint32_t _etext;
    extern uint32_t _estack;

    __attribute__((naked)) void HardFault_Handler()
    {
        __asm volatile(
            "tst  lr, #4          \n"
            "ite  eq              \n"
            "mrseq r0, msp        \n"
            "mrsne r0, psp        \n"
            "mov  r1, lr          \n"
            "b    HardFault_Handler_C \n");
    }

    void HardFault_Handler_C(const uint32_t* stack, uint32_t lrValue)
    {
        using application::PersistentFaultData;
        using application::persistentFaultData;

        // Invalidate while writing to prevent partially-written data being
        // read as valid on a watchdog timeout mid-write.
        persistentFaultData.magic = 0u;

        // ARM exception stack frame: R0, R1, R2, R3, R12, LR, PC, xPSR
        persistentFaultData.r0 = stack[0];
        persistentFaultData.r1 = stack[1];
        persistentFaultData.r2 = stack[2];
        persistentFaultData.r3 = stack[3];
        persistentFaultData.r12 = stack[4];
        persistentFaultData.lr = stack[5];
        persistentFaultData.pc = stack[6];
        persistentFaultData.psr = stack[7];

        // Cortex-M fault status registers
        persistentFaultData.cfsr = SCB->CFSR;
        persistentFaultData.mmfar = SCB->MMFAR;
        persistentFaultData.bfar = SCB->BFAR;

        // Stack trace: scan above the exception frame for code addresses
        const uint32_t textStart = reinterpret_cast<uint32_t>(&_text);
        const uint32_t textEnd = reinterpret_cast<uint32_t>(&_etext);
        const uint32_t* sp = stack + 8u; // skip 8-word exception frame
        const uint32_t* stackTop = &_estack;
        uint32_t count = 0u;

        while (sp < stackTop && count < PersistentFaultData::kMaxStackTraceEntries)
        {
            // ARM THUMB return addresses have bit 0 set; mask it for range check
            const uint32_t addr = (*sp) & 0xFFFFFFFEu;
            if (addr >= textStart && addr < textEnd)
                persistentFaultData.stackTrace[count++] = *sp;
            ++sp;
        }
        persistentFaultData.stackTraceCount = count;

        // Write magic last — this atomically marks data as valid
        persistentFaultData.magic = PersistentFaultData::kMagicValid;

        // Trigger reset via abort — platform startup (DefaultInit / HAL) maps
        // abort() to NVIC_SystemReset() or equivalent.
        std::abort();
    }
}
