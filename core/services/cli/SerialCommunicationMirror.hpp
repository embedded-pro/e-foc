#pragma once

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"

namespace services
{
    // Presents two links as one: everything sent goes out on both, and input from either is delivered. It lets the console
    // stay on the UART while a debug probe reads and drives the same console over RTT, so a dropped USB-serial adapter
    // does not cut off the only way to talk to the board.
    class SerialCommunicationMirror
        : public hal::SerialCommunication
    {
    public:
        SerialCommunicationMirror(hal::SerialCommunication& primary, hal::SerialCommunication& secondary);

        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        hal::SerialCommunication& primary;
        hal::SerialCommunication& secondary;
    };
}
