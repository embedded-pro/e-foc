#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"

namespace sil
{
    class SemihostingCan
        : public hal::Can
    {
    public:
        SemihostingCan();

        void SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone) override;
        void ReceiveData(const infra::Function<void(Id, const Message&)>& onReceived) override;

        void PollIncoming();

    private:
        infra::Function<void(Id, const Message&)> receiveCallback;
        bool nonBlockingSet{ false };
    };
}
