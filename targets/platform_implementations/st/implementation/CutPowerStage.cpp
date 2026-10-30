#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"
#include DEVICE_HEADER

namespace application
{
    void CutPowerStage()
    {
        if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) == 0u)
            return;

        TIM1->BDTR &= ~TIM_BDTR_MOE;
    }
}
