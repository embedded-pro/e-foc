#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace
{
    hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType, uint16_t nodeId)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, nodeId));
    }

    int HexValue(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    // "CAN_TX <id-hex> <payload-hex>", the form the simulated target prints every frame it sends in.
    bool ParseTransmittedFrame(const std::string& line, hal::Can::Id expectedId, hal::Can::Message& out)
    {
        static const std::string prefix{ "CAN_TX " };
        if (line.rfind(prefix, 0) != 0)
            return false;

        const auto idEnd = line.find(' ', prefix.size());
        if (idEnd == std::string::npos)
            return false;

        uint32_t rawId = 0;
        for (std::size_t i = prefix.size(); i < idEnd; ++i)
        {
            const int digit = HexValue(line[i]);
            if (digit < 0)
                return false;
            rawId = (rawId << 4) | static_cast<uint32_t>(digit);
        }

        if (hal::Can::Id::Create29BitId(rawId) != expectedId)
            return false;

        out.clear();
        for (std::size_t i = idEnd + 1; i + 1 < line.size() && !out.full(); i += 2)
        {
            const int hi = HexValue(line[i]);
            const int lo = HexValue(line[i + 1]);
            if (hi < 0 || lo < 0)
                break;
            out.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }

        return true;
    }
}

namespace integration
{
    Fixture::Fixture()
        : interactor{ TargetInteractor::Instance() }
    {}

    void Fixture::SyncLines()
    {
        allLines = interactor.SerialLines();
        lastResponse = interactor.LastSerialLine();
        lastCommandDuration = interactor.LastSerialDuration();
    }

    bool Fixture::SendCommand(const std::string& command, std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.SendCommand(command, timeout);
        SyncLines();
        return ok;
    }

    bool Fixture::DrainLines(std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.DrainSerial(timeout);
        SyncLines();
        return ok;
    }

    bool Fixture::FlushPartialLines(std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.DrainSerial(timeout);
        SyncLines();
        return ok;
    }

    bool Fixture::WaitForPrompt(std::chrono::milliseconds timeout)
    {
        const bool ok = interactor.WaitForPrompt(timeout);
        SyncLines();
        return ok;
    }

    bool Fixture::WaitForCanHeartbeat(std::chrono::milliseconds timeout)
    {
        const hal::Can::Id heartbeatId = MakeId(services::CanPriority::heartbeat,
            services::canSystemCategoryId, services::canHeartbeatMessageTypeId, kServerNodeId);
        hal::Can::Message payload;
        std::chrono::milliseconds elapsed{ 0 };
        return WaitForCanFrame(heartbeatId, payload, timeout, elapsed);
    }

    bool Fixture::SendCanCommand(uint8_t category, uint8_t messageType,
        const hal::Can::Message& extra, std::chrono::milliseconds timeout)
    {
        const hal::Can::Id commandId = MakeId(services::CanPriority::command,
            category, messageType, kServerNodeId);
        hal::Can::Message payload;
        payload.push_back(nextSequence);
        for (const uint8_t b : extra)
        {
            if (payload.full())
                break;
            payload.push_back(b);
        }
        if (!interactor.SendCanFrame(commandId, payload, std::chrono::milliseconds{ 100 }))
            return false;

        const hal::Can::Id ackId = MakeId(services::CanPriority::response,
            services::canSystemCategoryId, services::canCommandAckMessageTypeId, kServerNodeId);

        // Loop to skip stale ACKs left in the pipe from previous WaitForMotorState telemetry polls.
        // The firmware ACK payload is [category, messageType, status, expectedSeq]; we must verify
        // ackPayload[1] == messageType so we don't mistake a telemetry-request ACK for our command.
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            hal::Can::Message ackPayload;
            std::chrono::milliseconds elapsed{ 0 };
            if (!WaitForCanFrame(ackId, ackPayload, remaining, elapsed))
                return false;

            if (ackPayload.size() < 3 || ackPayload[1] != messageType)
                continue;

            ++nextSequence;
            return ackPayload[2] == static_cast<uint8_t>(services::CanAckStatus::success);
        }
    }

    bool Fixture::WaitForMotorState(can::FocMotorState expectedState, std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, kServerNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, kServerNodeId);

        static constexpr auto kMinPollInterval = std::chrono::milliseconds{ 20 };

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto pollStart = std::chrono::steady_clock::now();
            hal::Can::Message request;
            request.push_back(nextSequence++);
            interactor.SendCanFrame(requestId, request, std::chrono::milliseconds{ 100 });

            hal::Can::Message payload;
            std::chrono::milliseconds elapsed{ 0 };
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                break;
            if (WaitForCanFrame(telemetryId, payload, std::min(remaining, std::chrono::milliseconds{ 1000 }), elapsed) && !payload.empty())
            {
                if (std::getenv("SIL_VERBOSE") != nullptr)
                    std::fprintf(stderr, "[Fixture] WaitForMotorState: got state=%d expected=%d elapsed=%ldms\n",
                        static_cast<int>(payload[0]),
                        static_cast<int>(expectedState),
                        static_cast<long>(elapsed.count()));
                if (static_cast<can::FocMotorState>(payload[0]) == expectedState)
                    return true;
            }

            const auto pollElapsed = std::chrono::steady_clock::now() - pollStart;
            if (pollElapsed < kMinPollInterval)
            {
                const auto sleepMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    kMinPollInterval - pollElapsed);
                if (sleepMs > std::chrono::milliseconds{ 0 })
                    usleep(static_cast<useconds_t>(sleepMs.count() * 1000));
            }
        }
        return false;
    }

    bool Fixture::WaitForFaultCode(can::FocFaultCode expectedFault, std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, kServerNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, kServerNodeId);

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            hal::Can::Message request;
            request.push_back(nextSequence++);
            interactor.SendCanFrame(requestId, request, std::chrono::milliseconds{ 100 });

            hal::Can::Message payload;
            std::chrono::milliseconds elapsed{ 0 };
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                break;

            if (WaitForCanFrame(telemetryId, payload, std::min(remaining, std::chrono::milliseconds{ 1000 }), elapsed) && payload.size() >= 2)
            {
                if (std::getenv("SIL_VERBOSE") != nullptr)
                    std::fprintf(stderr, "[Fixture] WaitForFaultCode: got fault=%d expected=%d\n",
                        static_cast<int>(payload[1]), static_cast<int>(expectedFault));
                if (static_cast<can::FocFaultCode>(payload[1]) == expectedFault)
                    return true;
            }

            usleep(20000);
        }
        return false;
    }

    std::optional<float> Fixture::ReadMeasuredPosition(std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, kServerNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, kServerNodeId);

        hal::Can::Message request;
        request.push_back(nextSequence++);
        interactor.SendCanFrame(requestId, request, std::chrono::milliseconds{ 100 });

        hal::Can::Message payload;
        std::chrono::milliseconds elapsed{ 0 };
        if (!WaitForCanFrame(telemetryId, payload, timeout, elapsed) || payload.size() < 6)
            return std::nullopt;

        const auto raw = static_cast<int16_t>((static_cast<uint16_t>(payload[4]) << 8) | payload[5]);
        return static_cast<float>(raw) / static_cast<float>(can::focPositionScale);
    }

    std::optional<float> Fixture::ReadMeasuredSpeed(std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, kServerNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, kServerNodeId);

        hal::Can::Message request;
        request.push_back(nextSequence++);
        interactor.SendCanFrame(requestId, request, std::chrono::milliseconds{ 100 });

        hal::Can::Message payload;
        std::chrono::milliseconds elapsed{ 0 };
        if (!WaitForCanFrame(telemetryId, payload, timeout, elapsed) || payload.size() < 4)
            return std::nullopt;

        const auto raw = static_cast<int16_t>((static_cast<uint16_t>(payload[2]) << 8) | payload[3]);
        return static_cast<float>(raw) / static_cast<float>(can::focSpeedScale);
    }

    std::optional<can::FocMotorState> Fixture::ReadMotorState(std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, kServerNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, kServerNodeId);

        hal::Can::Message request;
        request.push_back(nextSequence++);
        interactor.SendCanFrame(requestId, request, std::chrono::milliseconds{ 100 });

        hal::Can::Message payload;
        std::chrono::milliseconds elapsed{ 0 };
        if (!WaitForCanFrame(telemetryId, payload, timeout, elapsed) || payload.empty())
            return std::nullopt;

        return static_cast<can::FocMotorState>(payload[0]);
    }

    bool Fixture::SelectControlMode(can::FocMotorMode mode, std::chrono::milliseconds timeout)
    {
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(mode));
        if (!SendCanCommand(can::focMotorCategoryId, can::focSelectControlModeId, payload, timeout))
            return false;
        return WaitForMotorState(can::FocMotorState::idle, timeout);
    }

    bool Fixture::EnableMotor(std::chrono::milliseconds timeout)
    {
        if (!SendCanCommand(can::focMotorCategoryId, can::focStartId, {}, timeout))
            return false;
        return WaitForMotorState(can::FocMotorState::running, timeout);
    }

    bool Fixture::DisableMotor(std::chrono::milliseconds timeout)
    {
        if (!SendCanCommand(can::focMotorCategoryId, can::focStopId, {}, timeout))
            return false;
        return WaitForMotorState(can::FocMotorState::idle, timeout);
    }

    bool Fixture::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds timeout)
    {
        return interactor.SendCanFrame(id, message, timeout);
    }

    bool Fixture::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& outPayload,
        std::chrono::milliseconds timeout, std::chrono::milliseconds& outElapsed)
    {
        return interactor.WaitForCanFrame(expectedId, outPayload, timeout, canReference, outElapsed);
    }

    void Fixture::StartCanCapture()
    {
        interactor.ClearCanFrames();
        canReference = {};
    }

    void Fixture::MarkCanReference()
    {
        canReference = std::chrono::steady_clock::now();
    }

    std::optional<hal::Can::Message> Fixture::FindCapturedCanFrame(hal::Can::Id id, std::size_t fromLine) const
    {
        const auto& lines = interactor.SerialLines();
        std::optional<hal::Can::Message> found;

        for (std::size_t i = fromLine; i < lines.size(); ++i)
        {
            hal::Can::Message payload;
            if (ParseTransmittedFrame(lines[i], id, payload))
                found = payload;
        }

        return found;
    }

    std::size_t Fixture::CapturedLineCount() const
    {
        return interactor.SerialLines().size();
    }
}
