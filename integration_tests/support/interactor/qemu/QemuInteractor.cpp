#include "integration_tests/support/interactor/qemu/QemuInteractor.hpp"
#include <algorithm>
#include <chrono>
#include <utility>

namespace sil
{
    QemuInteractor::QemuInteractor(std::string elfPath)
        : elfPath{ std::move(elfPath) }
    {}

    void QemuInteractor::Setup() {}

    void QemuInteractor::Teardown() {}

    void QemuInteractor::BeforeScenario()
    {
        lines.clear();
        lastLine.clear();
        session.Start(elfPath);
    }

    void QemuInteractor::AfterScenario()
    {
        session.Stop();
    }

    bool QemuInteractor::SendCommand(const std::string& command, std::chrono::milliseconds timeout)
    {
        if (!session.IsRunning())
            return false;
        if (!session.SendLine(command))
            return false;
        std::string out;
        const bool ok = session.WaitFor("DONE", out, timeout);
        if (ok)
        {
            lines.push_back(out);
            lastLine = out;
        }
        return ok;
    }

    bool QemuInteractor::DrainSerial(std::chrono::milliseconds timeout)
    {
        if (!session.IsRunning())
            return true;
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::string line;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                break;
            if (!session.ReadLine(line, std::min(remaining, std::chrono::milliseconds{ 50 })))
                break;
            lines.push_back(line);
            lastLine = line;
        }
        return true;
    }

    bool QemuInteractor::WaitForPrompt(std::chrono::milliseconds)
    {
        return true;
    }

    void QemuInteractor::ClearSerialLines()
    {
        lines.clear();
        lastLine.clear();
    }

    const std::vector<std::string>& QemuInteractor::SerialLines() const
    {
        return lines;
    }

    const std::string& QemuInteractor::LastSerialLine() const
    {
        return lastLine;
    }

    std::chrono::milliseconds QemuInteractor::LastSerialDuration() const
    {
        return std::chrono::milliseconds{ 0 };
    }

    void QemuInteractor::ClearCanFrames() {}

    bool QemuInteractor::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds)
    {
        if (!session.IsRunning())
            return false;
        return session.SendCanFrame(id, message);
    }

    bool QemuInteractor::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
        std::chrono::milliseconds timeout,
        std::chrono::steady_clock::time_point,
        std::chrono::milliseconds& elapsed)
    {
        if (!session.IsRunning())
            return false;
        const auto start = std::chrono::steady_clock::now();
        const bool found = session.WaitForCanFrame(expectedId, out, timeout);
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        return found;
    }
}
