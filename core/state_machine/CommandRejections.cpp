#include "core/state_machine/CommandRejections.hpp"
#include <type_traits>
#include <variant>

namespace application
{
    namespace
    {
        template<class T>
        concept CarriesCallback = requires(const T& event) { event.onDone.Callback(); };
    }

    CommandRejections::CommandRejections(services::StateMachine<state_machine::State, state_machine::Event>& subject)
        : services::StateMachineObserver<state_machine::State, state_machine::Event>(subject)
    {}

    void CommandRejections::StateChanged(StateId, const state_machine::Event&, StateId)
    {}

    void CommandRejections::EventForbidden(StateId, const state_machine::Event& event)
    {
        Reject(event);
    }

    void CommandRejections::EventRejected(StateId, const state_machine::Event& event)
    {
        Reject(event);
    }

    void CommandRejections::Reject(const state_machine::Event& event)
    {
        std::visit([](const auto& command)
            {
                if constexpr (CarriesCallback<std::remove_cvref_t<decltype(command)>>)
                    command.onDone.Callback()(state_machine::CommandResult::rejected);
            },
            event);
    }
}
