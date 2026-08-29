#pragma once

#include "hal/cortex_m/InterruptCortex.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace application
{
    class QemuTimer
        : private hal::cortex::InterruptHandler
    {
    public:
        QemuTimer(uint32_t baseAddress,
            int32_t irqNumber,
            uint32_t systemClockHz,
            uint32_t targetHz,
            const infra::Function<void()>& onTick);

        void Start();
        void Stop();

    private:
        void Invoke() override;

        struct Registers
        {
            volatile uint32_t ctrl;
            volatile uint32_t value;
            volatile uint32_t reload;
            volatile uint32_t intclr;
        };

        Registers& regs;
        infra::Function<void()> onTick;
    };
}
