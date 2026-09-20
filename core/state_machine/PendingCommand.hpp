#pragma once

#include "core/state_machine/FocStateMachineEvents.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <optional>

namespace application
{
    class PendingCommand
    {
    public:
        void Accept(const state_machine::CommandCallback& callback);
        bool Pending() const;

        void Complete(state_machine::CommandResult result);
        void CompleteAfterTransition(state_machine::CommandResult result);
        void FlushDeferred();

    private:
        infra::AutoResetFunction<void(state_machine::CommandResult)> callback;
        std::optional<state_machine::CommandResult> deferred;
    };
}
