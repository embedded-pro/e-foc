#include "targets/platform_implementations/qemu/implementation/QemuTimer.hpp"

namespace
{
    constexpr uint32_t ctrlEnable = 1u << 0;
    constexpr uint32_t ctrlIrqEnable = 1u << 3;
}

namespace application
{
    QemuTimer::QemuTimer(uint32_t baseAddress,
        int32_t irqNumber,
        uint32_t systemClockHz,
        uint32_t targetHz,
        const infra::Function<void()>& onTick)
        : regs(*reinterpret_cast<Registers*>(baseAddress))
        , onTick(onTick)
    {
        regs.ctrl = 0u;
        regs.reload = systemClockHz / targetHz - 1u;
        regs.value = regs.reload;
        Register(irqNumber, hal::cortex::InterruptPriority::normal);
    }

    void QemuTimer::Start()
    {
        regs.ctrl = ctrlEnable | ctrlIrqEnable;
    }

    void QemuTimer::Stop()
    {
        regs.ctrl = 0u;
    }

    void QemuTimer::Invoke()
    {
        regs.intclr = 1u;
        onTick();
    }
}
