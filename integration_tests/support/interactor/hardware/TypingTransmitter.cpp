#include "integration_tests/support/interactor/hardware/TypingTransmitter.hpp"
#include "infra/util/ByteRange.hpp"
#include <memory>

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
        struct PendingByte
        {
            uint8_t byte;
            bool completed{ false };
        };

        // The send can still be in flight after the retries give up, so it shares ownership of the byte and the flag
        auto pending = std::make_shared<PendingByte>(PendingByte{ byte });
        communication.SendData(infra::MakeRangeFromSingleObject(pending->byte), [pending]
            {
                pending->completed = true;
            });

        runFor(interCharDelay);

        for (int attempts = 0; attempts < 20 && !pending->completed; ++attempts)
            runFor(interCharDelay);
    }
}
