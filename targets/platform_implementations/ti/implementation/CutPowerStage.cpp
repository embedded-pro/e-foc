#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"
#include DEVICE_HEADER
#include <cstdint>

namespace application
{
    namespace
    {
        void CutModule(uint8_t pwmIndex, uint32_t base)
        {
            if ((SYSCTL->PRPWM & (1u << pwmIndex)) == 0u)
                return;

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
