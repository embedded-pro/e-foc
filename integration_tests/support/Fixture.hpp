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
            std::size_t frame;
        };

        // A frame the target sent while another wait was reading stays captured: both interactors keep every
        // frame they receive. This finds the last one with the given id after the mark.
        std::optional<hal::Can::Message> FindCapturedCanFrame(hal::Can::Id id, CaptureMark from) const;
        std::optional<hal::Can::Message> WaitForCapturedCanFrame(hal::Can::Id id, CaptureMark from, std::chrono::milliseconds timeout);
        CaptureMark MarkCapture() const;

        uint8_t nextSequence{ 0 };

    private:
        void RequestStatus();
        void SendClientHeartbeat();
        std::optional<uint16_t> FindHeartbeatNode(CaptureMark from) const;

        TargetInteractor& interactor;
        std::chrono::steady_clock::time_point canReference{};
        uint16_t serverNodeId{ 1 };
    };
}
