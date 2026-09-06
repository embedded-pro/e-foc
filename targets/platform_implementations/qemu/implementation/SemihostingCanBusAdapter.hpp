#pragma once

#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "infra/util/Function.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingCan.hpp"

namespace application
{
    class SemihostingCanBusAdapter
        : public CanBusAdapter
    {
    public:
        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        void SetOnError(const infra::Function<void(CanError)>& handler) override;

        void NotifyError(CanError error);

        void PollIncoming();

    private:
        sil::SemihostingCan can;
        infra::Function<void(CanError)> onError;
    };
}
