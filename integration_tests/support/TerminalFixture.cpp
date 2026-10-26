#include "integration_tests/support/TerminalFixture.hpp"

namespace integration
{
    TerminalFixture::TerminalFixture(TerminalInteractor& interactor)
        : interactor{ interactor }
    {}

    void TerminalFixture::SyncLines()
    {
        allLines = interactor.SerialLines();
        lastResponse = interactor.LastSerialLine();
        lastCommandDuration = interactor.LastSerialDuration();
    }

    bool TerminalFixture::SendCommand(const std::string& command, std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.SendCommand(command, timeout);
        SyncLines();
        return ok;
    }

    bool TerminalFixture::DrainLines(std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.DrainSerial(timeout);
        SyncLines();
        return ok;
    }

    bool TerminalFixture::FlushPartialLines(std::chrono::milliseconds timeout)
    {
        const auto linesBeforeFlush = allLines;
        const bool ok = interactor.DrainSerial(timeout);
        SyncLines();
        allLines.insert(allLines.begin(), linesBeforeFlush.begin(), linesBeforeFlush.end());
        return ok;
    }

    bool TerminalFixture::WaitForPrompt(std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.WaitForPrompt(timeout);
        SyncLines();
        return ok;
    }

    bool TerminalFixture::WaitForRestart(const std::string& bannerLine, std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.WaitForRestart(bannerLine, timeout);
        SyncLines();
        return ok;
    }

    std::size_t TerminalFixture::CapturedLineCount() const
    {
        return interactor.DiscardedSerialLineCount() + interactor.SerialLines().size();
    }

    std::optional<std::string> TerminalFixture::LastCapturedLineWith(std::size_t fromMark, const std::string& needle) const
    {
        const auto& lines = interactor.SerialLines();
        std::optional<std::string> found;

        for (std::size_t i = LineIndexOf(fromMark); i < lines.size(); ++i)
            if (lines[i].find(needle) != std::string::npos)
                found = lines[i];

        return found;
    }

    std::size_t TerminalFixture::LineIndexOf(std::size_t mark) const
    {
        const auto discarded = interactor.DiscardedSerialLineCount();
        return mark > discarded ? mark - discarded : 0;
    }
}
