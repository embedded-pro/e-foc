#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"

namespace supervision
{
    struct ExpectedSources
    {
        bool innerLoop{ false };
        bool outerLoop{ false };
    };

    class ControlHealth
    {
    public:
        static ExpectedSources Expected(state_machine::ControlMode mode, const state_machine::State& state);

        foc::ProgressSignal& InnerLoopProgress();
        foc::ProgressSignal& OuterLoopProgress();

        bool TakeEligibility(state_machine::ControlMode mode, const state_machine::State& state);

    private:
        foc::ProgressSignal innerLoop;
        foc::ProgressSignal outerLoop;
    };
}
