#include "integration_tests/support/Fixture.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <unistd.h>

namespace
{
    float ReadFloat32BE(const hal::Can::Message& payload, std::size_t offset)
    {
        const auto bits = (static_cast<uint32_t>(payload[offset]) << 24) | (static_cast<uint32_t>(payload[offset + 1]) << 16) |
                          (static_cast<uint32_t>(payload[offset + 2]) << 8) | static_cast<uint32_t>(payload[offset + 3]);
        return std::bit_cast<float>(bits);
    }

    hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType, uint16_t nodeId)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, nodeId));
    }

    constexpr auto kHeartbeatSlice = std::chrono::milliseconds{ 50 };
    constexpr auto kStatusRequestInterval = std::chrono::seconds{ 1 };
    constexpr auto kCapturePollSlice = std::chrono::milliseconds{ 50 };

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

    bool Fixture::WaitForCanHeartbeat(std::chrono::milliseconds timeout)
    {
        const auto mark = MarkCapture();
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            const auto now = std::chrono::steady_clock::now();
            if (now - lastStatusRequest >= kStatusRequestInterval)
            {
                RequestStatus();
                lastStatusRequest = now;
            }

            const auto candidate = serverNodeId.value_or(kAssumedServerNodeId);
            const hal::Can::Id heartbeatId = MakeId(services::CanPriority::heartbeat,
                services::canSystemCategoryId, services::canHeartbeatMessageTypeId, candidate);
            hal::Can::Message payload;
            std::chrono::milliseconds elapsed{ 0 };
            if (WaitForCanFrame(heartbeatId, payload, std::min(remaining, kHeartbeatSlice), elapsed))
            {
                serverNodeId = candidate;
                return true;
            }

            if (const auto node = FindHeartbeatNode(mark); node.has_value())
            {
                serverNodeId = *node;
                return true;
            }
        }
    }

    uint16_t Fixture::ServerNodeId()
    {
        if (!serverNodeId.has_value() && !WaitForCanHeartbeat())
            serverNodeId = kAssumedServerNodeId;

        return *serverNodeId;
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
            category, messageType, ServerNodeId());
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
            services::canSystemCategoryId, services::canCommandAckMessageTypeId, ServerNodeId());

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

    void Fixture::RequestStatus()
    {
        const hal::Can::Id statusRequestId = MakeId(services::CanPriority::command,
            services::canSystemCategoryId, services::canStatusRequestMessageTypeId, services::canBroadcastNodeId);
        interactor.SendCanFrame(statusRequestId, {}, std::chrono::milliseconds{ 100 });
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
        return WaitForMotorState(std::initializer_list<can::FocMotorState>{ expectedState }, timeout);
    }

    std::optional<Fixture::TelemetrySample> Fixture::RequestTelemetry(std::chrono::milliseconds timeout)
    {
        const hal::Can::Id requestId = MakeId(services::CanPriority::command,
            can::focMotorCategoryId, can::focRequestTelemetryId, ServerNodeId());
        const hal::Can::Id statusId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryStatusResponseId, ServerNodeId());
        const hal::Can::Id motionId = MakeId(services::CanPriority::telemetry,
            can::focMotorCategoryId, can::focTelemetryMotionId, ServerNodeId());

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        hal::Can::Message request;
        request.push_back(nextSequence++);
        interactor.SendCanFrame(requestId, request, std::chrono::milliseconds{ 100 });

        hal::Can::Message status;
        std::chrono::milliseconds elapsed{ 0 };
        if (!WaitForCanFrame(statusId, status, timeout, elapsed) || status.size() != 2)
            return std::nullopt;

        // The motion frame follows the status frame of the same request; taking it here keeps a later read from finding it stale.
        hal::Can::Message motion;
        const auto remaining = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()), std::chrono::milliseconds{ 100 });
        if (!WaitForCanFrame(motionId, motion, remaining, elapsed) || motion.size() != 8)
            return std::nullopt;

        return TelemetrySample{ static_cast<can::FocMotorState>(status[0]), static_cast<can::FocFaultCode>(status[1]),
            ReadFloat32BE(motion, 0), ReadFloat32BE(motion, 4) };
    }

    bool Fixture::WaitForMotorState(std::initializer_list<can::FocMotorState> expectedStates, std::chrono::milliseconds timeout)
    {
        static constexpr auto kMinPollInterval = std::chrono::milliseconds{ 20 };

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto pollStart = std::chrono::steady_clock::now();
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - pollStart);
            if (remaining <= std::chrono::milliseconds{ 0 })
                break;

            if (const auto sample = RequestTelemetry(std::min(remaining, std::chrono::milliseconds{ 1000 })))
            {
                if (std::getenv("SIL_VERBOSE") != nullptr)
                    std::fprintf(stderr, "[Fixture] WaitForMotorState: got state=%d expected=%d\n",
                        static_cast<int>(sample->state), static_cast<int>(*expectedStates.begin()));
                if (std::ranges::find(expectedStates, sample->state) != expectedStates.end())
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
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                break;

            if (const auto sample = RequestTelemetry(std::min(remaining, std::chrono::milliseconds{ 1000 })))
            {
                if (std::getenv("SIL_VERBOSE") != nullptr)
                    std::fprintf(stderr, "[Fixture] WaitForFaultCode: got fault=%d expected=%d\n",
                        static_cast<int>(sample->fault), static_cast<int>(expectedFault));
                if (sample->fault == expectedFault)
                    return true;
            }

            usleep(20000);
        }
        return false;
    }

    std::optional<float> Fixture::ReadMeasuredPosition(std::chrono::milliseconds timeout)
    {
        const auto sample = RequestTelemetry(timeout);
        if (!sample)
            return std::nullopt;
        return sample->position;
    }

    std::optional<float> Fixture::ReadMeasuredSpeed(std::chrono::milliseconds timeout)
    {
        const auto sample = RequestTelemetry(timeout);
        if (!sample)
            return std::nullopt;
        return sample->speed;
    }

    std::optional<can::FocMotorState> Fixture::ReadMotorState(std::chrono::milliseconds timeout)
    {
        const auto sample = RequestTelemetry(timeout);
        if (!sample)
            return std::nullopt;
        return sample->state;
    }

    bool Fixture::SelectControlMode(can::FocMotorMode mode, std::chrono::milliseconds timeout)
    {
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(mode));
        if (!SendCanCommand(can::focMotorCategoryId, can::focSelectControlModeId, payload, timeout))
            return false;
        return WaitForMotorState({ can::FocMotorState::idle, can::FocMotorState::ready }, timeout);
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
        return WaitForMotorState(can::FocMotorState::ready, timeout);
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

    std::optional<hal::Can::Message> Fixture::WaitForCapturedCanFrame(hal::Can::Id id, CaptureMark from, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true)
        {
            if (auto found = FindCapturedCanFrame(id, from))
                return found;

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 })
                return std::nullopt;

            hal::Can::Message payload;
            std::chrono::milliseconds elapsed{ 0 };
            (void)WaitForCanFrame(id, payload, std::min(remaining, kCapturePollSlice), elapsed);
        }
    }

    std::vector<hal::Can::Message> Fixture::CollectCapturedCanFrames(hal::Can::Id id, CaptureMark from, std::size_t count, std::chrono::milliseconds timeout)
    {
        const auto neverSent = MakeId(services::CanPriority::heartbeat, services::canSystemCategoryId, services::canHeartbeatMessageTypeId, services::canBroadcastNodeId);
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            auto frames = interactor.FindReceivedCanFrames(id, from.frame);
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (frames.size() >= count || remaining <= std::chrono::milliseconds{ 0 })
                return frames;

            SendClientHeartbeat();
            hal::Can::Message payload;
            std::chrono::milliseconds elapsed{ 0 };
            (void)WaitForCanFrame(neverSent, payload, std::min(remaining, kClientHeartbeatInterval), elapsed);
        }
    }

    Fixture::CaptureMark Fixture::MarkCapture() const
    {
        return { interactor.ReceivedCanFrameCount() };
    }
}
