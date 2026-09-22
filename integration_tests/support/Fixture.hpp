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
        static constexpr uint16_t kServerNodeId = 1;

        Fixture();

        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool DrainLines(std::chrono::milliseconds timeout);
        bool FlushPartialLines(std::chrono::milliseconds timeout);
        bool WaitForPrompt(std::chrono::milliseconds timeout = std::chrono::milliseconds{ 1000 });

        bool WaitForCanHeartbeat(
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool SendCanCommand(uint8_t category, uint8_t messageType,
            const hal::Can::Message& payload = {},
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool WaitForMotorState(can::FocMotorState expectedState,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        bool WaitForFaultCode(can::FocFaultCode expectedFault,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });
        std::optional<float> ReadMeasuredPosition(
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

        // Frames the target sent while another wait was consuming the serial stream stay in the captured
        // lines; this finds the last one with the given id, from the line index the caller marked.
        std::optional<hal::Can::Message> FindCapturedCanFrame(hal::Can::Id id, std::size_t fromLine) const;
        std::size_t CapturedLineCount() const;

        std::string lastResponse;
        std::vector<std::string> allLines;
        std::chrono::milliseconds lastCommandDuration{ 0 };
        uint8_t nextSequence{ 0 };

    private:
        void SyncLines();

        TargetInteractor& interactor;
        std::chrono::steady_clock::time_point canReference{};
    };
}
