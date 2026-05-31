#pragma once

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace hil
{
    class TypingTransmitter
    {
    public:
        TypingTransmitter(hal::SerialCommunication& communication,
            const infra::Function<void(std::chrono::milliseconds)>& runFor,
            std::chrono::milliseconds interCharDelay);

        void Send(const std::string& payload);

    private:
        void SendOne(uint8_t byte);

        hal::SerialCommunication& communication;
        infra::Function<void(std::chrono::milliseconds)> runFor;
        std::chrono::milliseconds interCharDelay;
    };
}
