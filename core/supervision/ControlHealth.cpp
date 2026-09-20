#include "core/supervision/ControlHealth.hpp"

namespace supervision
{
    ExpectedSources ControlHealth::Expected(state_machine::ControlMode mode, const state_machine::State& state)
    {
        if (!std::holds_alternative<state_machine::Enabled>(state))
            return {};

        return { true, mode != state_machine::ControlMode::torque };
    }

    foc::ProgressSignal& ControlHealth::InnerLoopProgress()
    {
        return innerLoop;
    }

    foc::ProgressSignal& ControlHealth::OuterLoopProgress()
    {
        return outerLoop;
    }

    bool ControlHealth::TakeEligibility(state_machine::ControlMode mode, const state_machine::State& state)
    {
        const auto expected = Expected(mode, state);

        // Both signals are consumed unconditionally so that progress made before a state change is never
        // mistaken for progress made after it
        const auto innerProgressed = innerLoop.TakeProgress();
        const auto outerProgressed = outerLoop.TakeProgress();

        if (expected.innerLoop && !innerProgressed)
            return false;

        return !expected.outerLoop || outerProgressed;
    }
}
