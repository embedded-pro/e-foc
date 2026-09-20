#include "core/state_machine/PendingCommand.hpp"

namespace application
{
    void PendingCommand::Accept(const state_machine::CommandCallback& callback)
    {
        this->callback = callback.Callback();
    }

    bool PendingCommand::Pending() const
    {
        return callback != nullptr;
    }

    void PendingCommand::Complete(state_machine::CommandResult result)
    {
        if (callback != nullptr)
            callback(result);
    }

    void PendingCommand::CompleteAfterTransition(state_machine::CommandResult result)
    {
        deferred = result;
    }

    void PendingCommand::FlushDeferred()
    {
        if (!deferred.has_value())
            return;

        auto result = *deferred;
        deferred.reset();
        Complete(result);
    }
}
