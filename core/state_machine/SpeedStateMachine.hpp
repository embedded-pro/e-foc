#pragma once

#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/instantiations/FocController.hpp"
#include "core/state_machine/OuterLoopStateMachineFor.hpp"

namespace application
{
    class SpeedStateMachine
        : public OuterLoopStateMachineFor<foc::FocSpeedController, foc::FocSpeed>
    {
    public:
        using OuterLoopStateMachineFor::OuterLoopStateMachineFor;
    };
}
