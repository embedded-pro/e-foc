#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include <optional>

namespace sil
{
    class SemihostingCan
        : public hal::Can
    {
    public:
        SemihostingCan();

        void SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone) override;
        void ReceiveData(const infra::Function<void(Id, const Message&)>& onReceived) override;

        struct Frame
        {
            Id id;
            Message message;
        };

        std::optional<Frame> PollIncoming();

        // A line without the CAN prefix is a terminal command travelling on the same socket.
        void OnTerminalLine(const infra::Function<void(const char*)>& handler);

    private:
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(const char*)> terminalCallback;
    };
}
