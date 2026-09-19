#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"

namespace application
{
    void CutPowerStage()
    {
        // This platform's three-phase PWM is a stub: it owns no timer and drives no bridge, so there is
        // nothing to cut and no timer instance to name. Give this a body together with the driver that
        // replaces the stub — clearing TIM_BDTR_MOE on whichever advanced-control timer the board wires to
        // the gate drivers forces the outputs to their idle level in hardware.
    }
}
