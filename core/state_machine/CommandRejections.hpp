#pragma once

#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "services/fsm/StateMachine.hpp"

namespace application
{
    class CommandRejections
        : public services::StateMachineObserver<state_machine::State, state_machine::Event>
    {
    public:
        using StateId = services::AlternativeId<state_machine::State>;

        explicit CommandRejections(services::StateMachine<state_machine::State, state_machine::Event>& subject);

        void StateChanged(StateId from, const state_machine::Event& event, StateId to) override;
        void EventForbidden(StateId state, const state_machine::Event& event) override;
        void EventRejected(StateId state, const state_machine::Event& event) override;

    private:
        static void Reject(const state_machine::Event& event);
    };
}
