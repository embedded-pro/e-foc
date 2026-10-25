#include "targets/platform_implementations/qemu/implementation/EmulatedCanBusAdapter.hpp"

namespace application
{
    EmulatedCanBusAdapter::EmulatedCanBusAdapter(uintptr_t uartBase)
        : can(uartBase)
    {}

    void EmulatedCanBusAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool)>& actionOnCompletion)
    {
        can.SendData(id, data, actionOnCompletion);
    }

    void EmulatedCanBusAdapter::ReceiveData(const infra::Function<void(Id, const Message&)>& receivedAction)
    {
        can.ReceiveData(receivedAction);
    }

    void EmulatedCanBusAdapter::SetOnError(const infra::Function<void(CanError)>& handler)
    {
        onError = handler;
    }

    void EmulatedCanBusAdapter::NotifyError(CanError error)
    {
        RecordError(error);

        if (onError)
            onError(error);
    }

    std::optional<sil::EmulatedCan::Frame> EmulatedCanBusAdapter::PollIncoming()
    {
        return can.PollIncoming();
    }
}
