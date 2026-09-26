#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace integration;

namespace
{
    bool ContainsLine(const Fixture& fixture, const std::string& text)
    {
        for (const auto& line : fixture.allLines)
            if (line.find(text) != std::string::npos)
                return true;

        return false;
    }
}

WHEN(R"(the watchdog_stall command is sent to the hardware target)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog_stall", hil::timeouts::command))
        << "watchdog_stall command did not respond";
    ASSERT_TRUE(ContainsLine(fixture, "[WDT] stalling event loop"))
        << "watchdog_stall did not report that the event loop is about to stall";
}
