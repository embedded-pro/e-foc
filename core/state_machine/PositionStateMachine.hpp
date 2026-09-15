#pragma once

#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/instantiations/FocController.hpp"
#include "core/state_machine/OuterLoopStateMachineFor.hpp"

namespace application
{
    class PositionStateMachine
        : public OuterLoopStateMachineFor<foc::FocPositionController, foc::FocPosition>
    {
    public:
        using OuterLoopStateMachineFor::OuterLoopStateMachineFor;
    };
}
