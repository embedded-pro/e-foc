#pragma once

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"

namespace application
{
    class SemihostingSerial
        : public hal::SerialCommunication
    {
    public:
        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        infra::Function<void(infra::ConstByteRange)> onReceived;
    };
}
