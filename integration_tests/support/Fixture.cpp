#include "integration_tests/support/Fixture.hpp"
#include <chrono>

namespace
{
    hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType, uint16_t nodeId)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, nodeId));
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
        hal::Can::Message ackPayload;
        std::chrono::milliseconds elapsed{ 0 };
        if (!WaitForCanFrame(ackId, ackPayload, timeout, elapsed))
            return false;

        ++nextSequence;
        return ackPayload.size() >= 3 &&
               ackPayload[2] == static_cast<uint8_t>(services::CanAckStatus::success);
    }

    bool Fixture::WaitForMotorState(can::FocMotorState expectedState, std::chrono::milliseconds timeout)
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
            if (WaitForCanFrame(telemetryId, payload, std::min(remaining, std::chrono::milliseconds{ 1000 }), elapsed)
                && !payload.empty())
            {
                if (static_cast<can::FocMotorState>(payload[0]) == expectedState)
                    return true;
            }
        }
        return false;
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
}
