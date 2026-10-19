#pragma once

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"

namespace services
{
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
