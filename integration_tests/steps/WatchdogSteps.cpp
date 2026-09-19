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

WHEN(R"(the watchdog command is sent to the hardware target)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog", hil::timeouts::command))
        << "watchdog command did not respond";
}

WHEN(R"(the watchdog is enabled with a deadline of {int} ms)", (int deadlineMs))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog " + std::to_string(deadlineMs), hil::timeouts::command))
        << "watchdog command did not respond";
    ASSERT_TRUE(ContainsLine(fixture, "[WDT] enabled"))
        << "watchdog did not report that supervision started";
}

WHEN(R"(the watchdog_stall command is sent to the hardware target)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog_stall", hil::timeouts::command))
        << "watchdog_stall command did not respond";
    ASSERT_TRUE(ContainsLine(fixture, "[WDT] feeding stopped"))
        << "watchdog_stall did not report that feeding stopped";
}

THEN(R"(the watchdog command reports supervision is disabled)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog", hil::timeouts::command))
        << "watchdog command did not respond";
    EXPECT_TRUE(ContainsLine(fixture, "[WDT] disabled"))
        << "Expected '[WDT] disabled' in watchdog output";
}

THEN(R"(the watchdog command reports a deadline of {int} ms)", (int deadlineMs))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog", hil::timeouts::command))
        << "watchdog command did not respond";
    EXPECT_TRUE(ContainsLine(fixture, "[WDT] enabled deadline=" + std::to_string(deadlineMs) + "ms"))
        << "Expected the watchdog to report a deadline of " << deadlineMs << " ms";
}
