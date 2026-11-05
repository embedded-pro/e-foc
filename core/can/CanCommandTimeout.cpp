#include "core/can/CanCommandTimeout.hpp"
#include <variant>

namespace can
{
    CanCommandTimeout::CanCommandTimeout(const infra::Function<void(state_machine::FaultCode)>& onFault)
        : onFault{ onFault }
    {}

    void CanCommandTimeout::Enable(const infra::Function<infra::Duration()>& timeout)
    {
        this->timeout = timeout;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void CanCommandTimeout::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void CanCommandTimeout::CommandAccepted()
    {
        countedFrom = infra::Now();
    }

    void CanCommandTimeout::Evaluate()
    {
        if (activeDrive == nullptr)
            return;

        if (!std::holds_alternative<state_machine::Enabled>(activeDrive().CurrentState()))
        {
            enabled = false;
            reported = false;
            return;
        }

        const auto now = infra::Now();
        const auto limit = timeout();

        if (!enabled || limit <= infra::Duration::zero())
        {
            enabled = true;
            countedFrom = now;
            return;
        }

        if (reported || now - countedFrom < limit)
            return;

        reported = true;
        onFault(state_machine::FaultCode::commandTimeout);
    }
}
