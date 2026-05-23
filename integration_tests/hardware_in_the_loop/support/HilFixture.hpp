#pragma once

#include "hal/interfaces/Can.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeSession.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace hil
{
    struct HilFixture
    {
        HilFixture();
        ~HilFixture();

        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = timeouts::command);

        bool DrainLines(std::chrono::milliseconds timeout);

        bool FlushPartialLines(std::chrono::milliseconds timeout);

        bool WaitForPrompt(std::chrono::milliseconds timeout = timeouts::prompt);

        std::string lastResponse;
        std::vector<std::string> allLines;
        std::chrono::milliseconds lastCommandDuration{ 0 };

        void StartCanCapture();
        void MarkCanReference();

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 100 });

        bool WaitForCanFrame(hal::Can::Id expectedId,
            hal::Can::Message& outPayload,
            std::chrono::milliseconds timeout,
            std::chrono::milliseconds& outElapsed);

    private:
        BridgeSession& session;
        std::chrono::steady_clock::time_point canReference{};
    };
}
