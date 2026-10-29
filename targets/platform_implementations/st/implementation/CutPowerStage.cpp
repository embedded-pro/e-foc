#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"
#include DEVICE_HEADER

namespace application
{
    // Fault-handler safe: no driver state. With OSSI set, clearing MOE drives every gate input to its idle low level.
    void CutPowerStage()
    {
        if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) == 0u)
            return;

        TIM1->BDTR &= ~TIM_BDTR_MOE;
    }
}
