#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"
#include DEVICE_HEADER
#include <cstdint>

namespace application
{
    namespace
    {
        void CutModule(uint8_t pwmIndex, uint32_t base)
        {
            // A module the peripheral-ready register does not report drives nothing, and writing to one that
            // is unpowered raises a bus fault — a nested fault here is the lockup this call exists to
            // prevent. The gate is a hardware register, not application memory.
            if ((SYSCTL->PRPWM & (1u << pwmIndex)) == 0u)
                return;

            // Clearing PWMENABLE drives every output of the module low. It is the same register
            // hal::tiva::Pwm::Stop() clears per generator, reached without the driver object that owns it.
            reinterpret_cast<PWM0_Type*>(base)->ENABLE = 0u;
        }
    }

    void CutPowerStage()
    {
#if defined(PWM0_BASE)
        CutModule(0u, PWM0_BASE);
#endif
#if defined(PWM1_BASE)
        CutModule(1u, PWM1_BASE);
#endif
    }
}
