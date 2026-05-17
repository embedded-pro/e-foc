#include "integration_tests/hardware_in_the_loop/support/TypingTransmitter.hpp"
#include "infra/util/ByteRange.hpp"

namespace hil
{
    TypingTransmitter::TypingTransmitter(hal::SerialCommunication& communication,
        const infra::Function<void(std::chrono::milliseconds)>& runFor,
        std::chrono::milliseconds interCharDelay)
        : communication(communication)
        , runFor(runFor)
        , interCharDelay(interCharDelay)
    {}

    void TypingTransmitter::Send(const std::string& payload)
    {
        for (std::size_t i = 0; i < payload.size(); ++i)
        {
            SendOne(static_cast<uint8_t>(payload[i]));

            if (i + 1 < payload.size() && interCharDelay.count() > 0)
                runFor(interCharDelay);
        }
    }

    void TypingTransmitter::SendOne(uint8_t byte)
    {
        bool completed = false;
        communication.SendData(infra::MakeRangeFromSingleObject(byte), [&completed]
            {
                completed = true;
            });

        runFor(interCharDelay);

        for (int attempts = 0; attempts < 20 && !completed; ++attempts)
            runFor(interCharDelay);
    }
}
