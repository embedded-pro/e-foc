#pragma once

#include <cstdint>

namespace application
{
    // QEMU has no DWT, so CYCCNT reads zero; under -icount this free-running timer counts executed instructions.
    class QemuCycleCounter
    {
    public:
        QemuCycleCounter()
        {
            Timer().ctrl = 0u;
            Timer().reload = fullScale;
            Timer().value = fullScale;
            Timer().ctrl = ctrlEnable;
        }

        static uint32_t Now()
        {
            return fullScale - Timer().value;
        }

    private:
        struct Registers
        {
            volatile uint32_t ctrl;
            volatile uint32_t value;
            volatile uint32_t reload;
            volatile uint32_t intclr;
        };

        static constexpr uintptr_t timer1Base = 0x40001000u;
        static constexpr uint32_t ctrlEnable = 1u << 0;
        static constexpr uint32_t fullScale = 0xFFFFFFFFu;

        static Registers& Timer()
        {
            return *reinterpret_cast<Registers*>(timer1Base);
        }
    };
}
