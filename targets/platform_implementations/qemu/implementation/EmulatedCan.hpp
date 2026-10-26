#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "targets/platform_implementations/qemu/implementation/CmsdkUartLines.hpp"
#include <optional>

namespace sil
{
    class EmulatedCan
        : public hal::Can
    {
    public:
        explicit EmulatedCan(uintptr_t uartBase);

        void SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone) override;
        void ReceiveData(const infra::Function<void(Id, const Message&)>& onReceived) override;

        struct Frame
        {
            Id id;
            Message message;
        };

        std::optional<Frame> PollIncoming();

    private:
        CmsdkUartLines port;
        infra::Function<void(Id, const Message&)> receiveCallback;
    };
}
