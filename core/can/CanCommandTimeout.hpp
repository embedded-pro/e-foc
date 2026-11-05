#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace can
{
    class CanCommandTimeout
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };

        explicit CanCommandTimeout(const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<infra::Duration()>& timeout);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);
        void CommandAccepted();

    private:
        void Evaluate();

        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<infra::Duration()> timeout;
        infra::TimerRepeating evaluationTimer;
        infra::TimePoint countedFrom{};
        bool enabled{ false };
        bool reported{ false };
    };
}
