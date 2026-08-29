#pragma once
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilSession.hpp"
#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace sil
{
    struct QemuSilFixture
    {
        static constexpr uint16_t kServerNodeId = 1;

        QemuSilFixture();
        ~QemuSilFixture();

        bool WaitForCanHeartbeat(std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });

        bool SendCanCommand(uint8_t category, uint8_t messageType,
            const hal::Can::Message& payload = {},
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });

        bool WaitForMotorState(can::FocMotorState expectedState,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 100 });

        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& outPayload,
            std::chrono::milliseconds timeout, std::chrono::milliseconds& outElapsed);

        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });

        QemuSilSession session;
        std::map<std::string, uint32_t> cycleCounts;
        bool available{ false };
        uint8_t nextSequence{ 0 };
    };
}
