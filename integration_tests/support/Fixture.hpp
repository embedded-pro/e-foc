#pragma once

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace integration
{
    struct Fixture
    {
        // The target drops a client silent for 3 s of its own time, and a command such as an identification
        // acknowledges only when it ends: a waiting client heartbeats, as CanProtocolClient does. The simulated
        // target can run faster than the wall clock, so the interval leaves margin on the 3 s.
        static constexpr std::chrono::milliseconds kClientHeartbeatInterval{ 250 };

        Fixture();

        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool DrainLines(std::chrono::milliseconds timeout);
        bool FlushPartialLines(std::chrono::milliseconds timeout);
        bool WaitForPrompt(std::chrono::milliseconds timeout = std::chrono::milliseconds{ 1000 });

        bool WaitForCanHeartbeat(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        uint16_t ServerNodeId() const;
        bool SendCanCommand(uint8_t category, uint8_t messageType,
            const hal::Can::Message& payload = {},
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool WaitForMotorState(can::FocMotorState expectedState,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool WaitForFaultCode(can::FocFaultCode expectedFault,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        std::optional<float> ReadMeasuredPosition(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 2000 });
        std::optional<float> ReadMeasuredSpeed(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 2000 });
        std::optional<can::FocMotorState> ReadMotorState(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 2000 });
        bool SelectControlMode(can::FocMotorMode mode,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool EnableMotor(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool DisableMotor(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 100 });
        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& outPayload,
            std::chrono::milliseconds timeout,
            std::chrono::milliseconds& outElapsed);

        void StartCanCapture();
        void MarkCanReference();

        struct CaptureMark
        {
            std::size_t line;
            std::size_t frame;
        };

        // A frame the target sent while another wait was reading stays captured: the simulated target prints it
        // as a serial line, the bridge to real hardware keeps it. This finds the last one with the given id
        // after the mark.
        std::optional<hal::Can::Message> FindCapturedCanFrame(hal::Can::Id id, CaptureMark from) const;
        CaptureMark MarkCapture() const;
        // Counts every line the target printed, including those an interactor dropped since, so a mark taken
        // before a command still names the first line of its answer.
        std::size_t CapturedLineCount() const;
        std::optional<std::string> LastCapturedLineWith(std::size_t fromMark, const std::string& needle) const;

        std::string lastResponse;
        std::vector<std::string> allLines;
        std::chrono::milliseconds lastCommandDuration{ 0 };
        uint8_t nextSequence{ 0 };

    private:
        void SyncLines();
        void SendClientHeartbeat();
        std::size_t LineIndexOf(std::size_t mark) const;
        std::optional<uint16_t> FindHeartbeatNode(CaptureMark from) const;

        TargetInteractor& interactor;
        std::chrono::steady_clock::time_point canReference{};
        uint16_t serverNodeId{ 1 };
    };
}
