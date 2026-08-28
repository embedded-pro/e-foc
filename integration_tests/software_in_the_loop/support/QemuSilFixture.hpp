#pragma once
#include "integration_tests/software_in_the_loop/support/QemuSilSession.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace integration
{
    struct QemuSilFixture
    {
        QemuSilFixture();
        ~QemuSilFixture();

        void RunPerformanceBenchmark();

        uint32_t CycleCount(const std::string& key) const;

        QemuSilSession session;
        std::map<std::string, uint32_t> cycleCounts;
        bool available{ false };
    };
}
