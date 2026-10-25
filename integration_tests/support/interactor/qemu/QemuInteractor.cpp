#include "integration_tests/support/interactor/qemu/QemuInteractor.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <utility>

namespace sil
{
    QemuInteractor::QemuInteractor(std::string elfPath)
        : elfPath{ std::move(elfPath) }
    {}

    void QemuInteractor::Setup()
    {}

    void QemuInteractor::Teardown()
    {}

    void QemuInteractor::BeforeScenario()
    {
        lastLine.clear();
        session.ResetCapture();
        if (!session.CreateScenarioDirectory())
            GTEST_FAIL() << "[QEMU] could not create the scenario working directory";
        if (!session.Start(elfPath))
        {
            fprintf(stderr, "[QEMU] failed to start %s\n", elfPath.c_str());
            GTEST_FAIL() << "[QEMU] QEMU session failed to start";
        }
    }

    void QemuInteractor::AfterScenario()
    {
        session.Stop();
        session.RemoveScenarioDirectory();
    }

    bool QemuInteractor::SupportsSimulatedPlant() const
    {
        return true;
    }

    void QemuInteractor::ConfigurePlant(const std::vector<uint8_t>& record)
    {
        ASSERT_TRUE(session.WriteScenarioFile("plant.bin", record))
            << "[QEMU] could not write the plant description";
    }

    void QemuInteractor::ConfigureNonVolatileMemory(const std::vector<uint8_t>& image)
    {
        ASSERT_TRUE(session.WriteScenarioFile("eeprom.bin", image))
            << "[QEMU] could not write the NVM image";
    }

    void QemuInteractor::RestartTarget()
    {
        ClearSerialLines();
        ASSERT_TRUE(session.Restart(elfPath)) << "[QEMU] QEMU session failed to restart";
    }

    // The terminal answers through the tracer, so a step that needs the outcome drains the serial lines for
    // the trace it expects rather than waiting here.
    bool QemuInteractor::SendCommand(const std::string& command, std::chrono::milliseconds)
    {
        if (!session.IsRunning())
            return false;
        return session.SendTerminalLine(command);
    }

    bool QemuInteractor::DrainSerial(std::chrono::milliseconds timeout)
    {
        if (!session.IsRunning())
            return true;

        session.DrainDebugOutput(timeout);
        const auto& lines = session.CapturedLines();
        lastLine = lines.empty() ? std::string{} : lines.back();
        return true;
    }

    bool QemuInteractor::WaitForPrompt(std::chrono::milliseconds)
    {
        return true;
    }

    void QemuInteractor::ClearSerialLines()
    {
        discardedSerialLines += session.CapturedLines().size();
        lastLine.clear();
        session.ClearCapturedLines();
    }

    const std::vector<std::string>& QemuInteractor::SerialLines() const
    {
        return session.CapturedLines();
    }

    std::size_t QemuInteractor::DiscardedSerialLineCount() const
    {
        return discardedSerialLines;
    }

    const std::string& QemuInteractor::LastSerialLine() const
    {
        return lastLine;
    }

    std::chrono::milliseconds QemuInteractor::LastSerialDuration() const
    {
        return std::chrono::milliseconds{ 0 };
    }

    void QemuInteractor::ClearCanFrames()
    {}

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

    std::size_t QemuInteractor::ReceivedCanFrameCount() const
    {
        return session.ReceivedFrames().size();
    }

    std::optional<hal::Can::Message> QemuInteractor::FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const
    {
        const auto& frames = session.ReceivedFrames();
        std::optional<hal::Can::Message> found;

        for (std::size_t i = fromFrame; i < frames.size(); ++i)
            if (frames[i].id == id)
                found = frames[i].message;

        return found;
    }

    std::optional<hal::Can::Id> QemuInteractor::FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>& matches, std::size_t fromFrame) const
    {
        const auto& frames = session.ReceivedFrames();

        for (std::size_t i = fromFrame; i < frames.size(); ++i)
            if (matches(frames[i].id))
                return frames[i].id;

        return std::nullopt;
    }

    std::vector<std::string> QemuInteractor::TakePlantRecord(std::chrono::milliseconds timeout)
    {
        if (!session.IsRunning())
            return {};
        return session.TakePlantRecord(timeout);
    }

    std::string QemuInteractor::DebugLog() const
    {
        return session.DebugLogTail();
    }
}
