#include DEVICE_HEADER
#include <cstddef>
#include <cstdint>

// Strong definition of the weak hook the Cortex-M fault handler calls before it writes any
// diagnostics. It runs with the bridge in whatever state the faulting code left it, so it must
// reach the peripheral directly: no driver object, no allocation, no loop over live state.
//
// Clearing PWMENABLE drives every pwmN output low, which turns all six gate signals off, and it is
// one store per module. The module is only touched when SYSCTL reports it clocked - a write to an
// ungated peripheral faults, and a nested fault here is the lockup this hook exists to prevent.

namespace
{
#if defined(TM4C129)
    constexpr std::size_t numberOfPwms = 1;
#else
    constexpr std::size_t numberOfPwms = 2;
#endif

    constexpr uint32_t pwmBases[numberOfPwms] = {
        PWM0_BASE,
#if defined(TM4C123)
        PWM1_BASE,
#endif
    };
}

extern "C" void CutPowerStage(void)
{
    for (std::size_t index = 0; index != numberOfPwms; ++index)
        if ((SYSCTL->PRPWM & (1u << index)) != 0u)
            reinterpret_cast<PWM0_Type*>(pwmBases[index])->ENABLE = 0u;

    __DSB();
}
