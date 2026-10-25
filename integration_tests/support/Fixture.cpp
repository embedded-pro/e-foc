#include "integration_tests/support/Fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <unistd.h>

namespace
{
    hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType, uint16_t nodeId)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, nodeId));
    }

    constexpr auto kHeartbeatSlice = std::chrono::milliseconds{ 250 };

    bool IsServerHeartbeat(uint32_t rawId)
    {
        return services::ExtractCanPriority(rawId) == services::CanPriority::heartbeat &&
               services::ExtractCanCategory(rawId) == services::canSystemCategoryId &&
               services::ExtractCanMessageType(rawId) == services::canHeartbeatMessageTypeId &&
               services::ExtractCanNodeId(rawId) != services::canBroadcastNodeId;
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
        const auto linesBeforeFlush = allLines;
        const bool ok = interactor.DrainSerial(timeout);
        SyncLines();
        allLines.insert(allLines.begin(), linesBeforeFlush.begin(), linesBeforeFlush.end());
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
        const auto mark = MarkCapture();
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            const hal::Can::Id heartbeatId = MakeId(services::CanPriority::heartbeat,
                services::canSystemCategoryId, services::canHeartbeatMessageTypeId, serverNodeId);
            hal::Can::Message payload;
            std::chrono::milliseconds elapsed{ 0 };
            if (WaitForCanFrame(heartbeatId, payload, std::min(remaining, kHeartbeatSlice), elapsed))
                return true;

            if (const auto node = FindHeartbeatNode(mark); node.has_value())
            {
                serverNodeId = *node;
                return true;
            }
        }
    }

    uint16_t Fixture::ServerNodeId() const
    {
        return serverNodeId;
    }

    std::optional<uint16_t> Fixture::FindHeartbeatNode(CaptureMark from) const
    {
        const auto id = interactor.FindReceivedCanFrameId([](hal::Can::Id candidate)
            {
                return candidate.Is29BitId() && IsServerHeartbeat(candidate.Get29BitId());
            },
            from.frame);

        if (!id.has_value())
            return std::nullopt;

        return services::ExtractCanNodeId(id->Get29BitId());
    }

    bool Fixture::SendCanCommand(uint8_t category, uint8_t messageType,
        const hal::Can::Message& extra, std::chrono::milliseconds timeout)
    {
        const hal::Can::Id commandId = MakeId(services::CanPriority::command,
            category, messageType, serverNodeId);
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
            services::canSystemCategoryId, services::canCommandAckMessageTypeId, serverNodeId);

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
            const auto slice = std::min(remaining, kClientHeartbeatInterval);
            const auto sliceStart = std::chrono::steady_clock::now();
            if (!WaitForCanFrame(ackId, ackPayload, slice, elapsed))
            {
                // A wait that ends well inside its slice did not time out: the target stopped answering at all
                if (std::chrono::steady_clock::now() - sliceStart < slice / 2)
                    return false;

                SendClientHeartbeat();
                continue;
            }

            if (ackPayload.size() < 3 || ackPayload[1] != messageType)
                continue;

            ++nextSequence;
            return ackPayload[2] == static_cast<uint8_t>(services::CanAckStatus::success);
        }
    }

    void Fixture::SendClientHeartbeat()
    {
        const hal::Can::Id heartbeatId = MakeId(services::CanPriority::heartbeat,
            services::canSystemCategoryId, services::canHeartbeatMessageTypeId, services::canBroadcastNodeId);
        hal::Can::Message payload;
        payload.push_back(services::canProtocolVersion);
        interactor.SendCanFrame(heartbeatId, payload, std::chrono::milliseconds{ 100 });
    }

    bool Fixture::WaitForMotorState(can::FocMotorState expectedState, std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, serverNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, serverNodeId);

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
            can::focMotorCategoryId, can::focRequestTelemetryId, serverNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, serverNodeId);

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
            can::focMotorCategoryId, can::focRequestTelemetryId, serverNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, serverNodeId);

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
            can::focMotorCategoryId, can::focRequestTelemetryId, serverNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, serverNodeId);

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
            can::focMotorCategoryId, can::focRequestTelemetryId, serverNodeId);
        const hal::Can::Id telemetryId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, serverNodeId);

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

    std::optional<hal::Can::Message> Fixture::FindCapturedCanFrame(hal::Can::Id id, CaptureMark from) const
    {
        return interactor.FindReceivedCanFrame(id, from.frame);
    }

    Fixture::CaptureMark Fixture::MarkCapture() const
    {
        return { CapturedLineCount(), interactor.ReceivedCanFrameCount() };
    }

    std::size_t Fixture::CapturedLineCount() const
    {
        return interactor.DiscardedSerialLineCount() + interactor.SerialLines().size();
    }

    std::optional<std::string> Fixture::LastCapturedLineWith(std::size_t fromMark, const std::string& needle) const
    {
        const auto& lines = interactor.SerialLines();
        std::optional<std::string> found;

        for (std::size_t i = LineIndexOf(fromMark); i < lines.size(); ++i)
            if (lines[i].find(needle) != std::string::npos)
                found = lines[i];

        return found;
    }

    std::size_t Fixture::LineIndexOf(std::size_t mark) const
    {
        const auto discarded = interactor.DiscardedSerialLineCount();
        return mark > discarded ? mark - discarded : 0;
    }
}
