#pragma once

#include "integration_tests/support/interactor/interfaces/TerminalInteractor.hpp"
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace integration
{
    struct TerminalFixture
    {
        explicit TerminalFixture(TerminalInteractor& interactor);

        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool DrainLines(std::chrono::milliseconds timeout);
        bool FlushPartialLines(std::chrono::milliseconds timeout);
        bool WaitForPrompt(std::chrono::milliseconds timeout = std::chrono::milliseconds{ 1000 });

        // Counts every line the target printed, including those an interactor dropped since, so a mark taken
        // before a command still names the first line of its answer.
        std::size_t CapturedLineCount() const;
        std::optional<std::string> LastCapturedLineWith(std::size_t fromMark, const std::string& needle) const;

        std::string lastResponse;
        std::vector<std::string> allLines;
        std::chrono::milliseconds lastCommandDuration{ 0 };

    private:
        void SyncLines();
        std::size_t LineIndexOf(std::size_t mark) const;

        TerminalInteractor& interactor;
    };
}
