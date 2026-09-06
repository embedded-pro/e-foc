#include "targets/platform_implementations/qemu/implementation/SemihostingCanBusAdapter.hpp"

namespace application
{
    void SemihostingCanBusAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool)>& actionOnCompletion)
    {
        can.SendData(id, data, actionOnCompletion);
    }

    void SemihostingCanBusAdapter::ReceiveData(const infra::Function<void(Id, const Message&)>& receivedAction)
    {
        can.ReceiveData(receivedAction);
    }

    void SemihostingCanBusAdapter::SetOnError(const infra::Function<void(CanError)>& handler)
    {
        onError = handler;
    }

    void SemihostingCanBusAdapter::NotifyError(CanError error)
    {
        RecordError(error);

        if (onError)
            onError(error);
    }

    void SemihostingCanBusAdapter::PollIncoming()
    {
        can.PollIncoming();
    }
}
