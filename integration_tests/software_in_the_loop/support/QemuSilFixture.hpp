#pragma once
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
        QemuSilFixture();
        ~QemuSilFixture();

        void RunPerformanceBenchmark();

        uint32_t CycleCount(const std::string& key) const;

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 100 });

        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& outPayload,
            std::chrono::milliseconds timeout, std::chrono::milliseconds& outElapsed);

        bool SendCommand(const std::string& command,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{ 5000 });

        QemuSilSession session;
        std::map<std::string, uint32_t> cycleCounts;
        bool available{ false };
    };
}
