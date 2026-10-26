#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace integration
{
    class TerminalInteractor
    {
    public:
        virtual bool SendCommand(const std::string& command, std::chrono::milliseconds timeout) = 0;
        virtual bool DrainSerial(std::chrono::milliseconds timeout) = 0;
        virtual bool WaitForPrompt(std::chrono::milliseconds timeout) = 0;
        virtual bool WaitForRestart(const std::string& bannerLine, std::chrono::milliseconds timeout) = 0;
        virtual void ClearSerialLines() = 0;
        virtual const std::vector<std::string>& SerialLines() const = 0;
        // Lines dropped from SerialLines() since setup, so a line count taken before a drop still names the same line.
        virtual std::size_t DiscardedSerialLineCount() const = 0;
        virtual const std::string& LastSerialLine() const = 0;
        virtual std::chrono::milliseconds LastSerialDuration() const = 0;

        static TerminalInteractor* Instance();
        static void SetInstance(TerminalInteractor& instance);

    protected:
        ~TerminalInteractor() = default;
    };
}
