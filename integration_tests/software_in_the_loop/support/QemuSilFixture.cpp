#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <chrono>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace
{
    hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType, uint16_t nodeId)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, nodeId));
    }
}

namespace
{
    void ParseDwtLine(const std::string& line, std::map<std::string, uint32_t>& out)
    {
        // Format: "DWT key=value"
        if (line.rfind("DWT ", 0) != 0)
            return;

        const std::string payload = line.substr(4);
        const auto eq = payload.find('=');
        if (eq == std::string::npos)
            return;

        const std::string key = payload.substr(0, eq);
        const std::string val = payload.substr(eq + 1);
        try
        {
            out[key] = static_cast<uint32_t>(std::stoul(val));
        }
        catch (...)
        {}
    }

    void ReadUntilDone(sil::QemuSilSession& session, std::map<std::string, uint32_t>& counts)
    {
        const std::chrono::milliseconds lineTimeout{ 10000 };
        std::string line;

        while (session.ReadLine(line, lineTimeout))
        {
            ParseDwtLine(line, counts);
            if (line == "DONE")
                break;
        }
    }
}

namespace sil
{
    QemuSilFixture::QemuSilFixture()
    {
        const char* envPath = std::getenv("QEMU_SIL_ELF");
        const std::string elfPath = (envPath != nullptr) ? envPath : QEMU_SIL_ELF_DEFAULT_PATH;

        struct stat st{};
        if (stat(elfPath.c_str(), &st) != 0)
            return;

        if (!session.Start(elfPath))
            return;

        available = true;
    }

    QemuSilFixture::~QemuSilFixture()
    {
        session.Stop();
    }

    void QemuSilFixture::RunPerformanceBenchmark()
    {
        if (!available)
            return;

        session.SendLine("perf");
        ReadUntilDone(session, cycleCounts);
    }

    uint32_t QemuSilFixture::CycleCount(const std::string& key) const
    {
        const auto it = cycleCounts.find(key);
        if (it == cycleCounts.end())
            return 0;
        return it->second;
    }

    bool QemuSilFixture::WaitForCanHeartbeat(std::chrono::milliseconds timeout)
    {
        if (!available)
            return false;

        const hal::Can::Id heartbeatId = MakeId(services::CanPriority::heartbeat,
            services::canSystemCategoryId, services::canHeartbeatMessageTypeId, kServerNodeId);
        hal::Can::Message payload;
        std::chrono::milliseconds elapsed;
        return WaitForCanFrame(heartbeatId, payload, timeout, elapsed);
    }

    bool QemuSilFixture::SendCanCommand(uint8_t category, uint8_t messageType,
        const hal::Can::Message& extra, std::chrono::milliseconds timeout)
    {
        if (!available)
            return false;

        const hal::Can::Id commandId = MakeId(services::CanPriority::command, category, messageType, kServerNodeId);
        hal::Can::Message payload;
        payload.push_back(nextSequence);
        for (const uint8_t b : extra)
        {
            if (payload.full())
                break;
            payload.push_back(b);
        }
        if (!session.SendCanFrame(commandId, payload))
            return false;

        const hal::Can::Id ackId = MakeId(services::CanPriority::response,
            services::canSystemCategoryId, services::canCommandAckMessageTypeId, kServerNodeId);
        hal::Can::Message ackPayload;
        std::chrono::milliseconds elapsed;
        if (!WaitForCanFrame(ackId, ackPayload, timeout, elapsed))
            return false;

        ++nextSequence;
        return ackPayload.size() >= 3 && ackPayload[2] == static_cast<uint8_t>(services::CanAckStatus::success);
    }

    bool QemuSilFixture::WaitForMotorState(can::FocMotorState expectedState, std::chrono::milliseconds timeout)
    {
        if (!available)
            return false;

        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, kServerNodeId);

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            hal::Can::Message payload;
            std::chrono::milliseconds elapsed;
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                break;
            if (WaitForCanFrame(telemetryId, payload, remaining, elapsed) && !payload.empty())
            {
                if (static_cast<can::FocMotorState>(payload[0]) == expectedState)
                    return true;
            }
        }
        return false;
    }

    bool QemuSilFixture::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds /*timeout*/)
    {
        if (!available)
            return false;
        return session.SendCanFrame(id, message);
    }

    bool QemuSilFixture::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& outPayload,
        std::chrono::milliseconds timeout, std::chrono::milliseconds& outElapsed)
    {
        if (!available)
            return false;

        const auto start = std::chrono::steady_clock::now();
        const bool found = session.WaitForCanFrame(expectedId, outPayload, timeout);
        outElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        return found;
    }

    bool QemuSilFixture::SendCommand(const std::string& command,
        std::chrono::milliseconds timeout)
    {
        if (!available)
            return false;

        if (!session.SendLine(command))
            return false;

        std::string line;
        return session.WaitFor("DONE", line, timeout);
    }
}
