#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <chrono>
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

    bool WaitForLine(Fixture& fixture, const std::string& text, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!ContainsLine(fixture, text))
        {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            (void)fixture.DrainLines(std::chrono::milliseconds{ 500 });
        }

        return true;
    }
}

GIVEN(R"(the emulated target is running)")
{
    ASSERT_TRUE(context.Get<Fixture>().WaitForCanHeartbeat()) << "emulated target did not send a heartbeat";
}

WHEN(R"(the event loop of the emulated target is stalled)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog_stall")) << "watchdog_stall could not be sent";
    ASSERT_TRUE(WaitForLine(fixture, "[WDT] stalling event loop", std::chrono::seconds{ 10 }))
        << "watchdog_stall did not report that the event loop is about to stall";
}

THEN(R"(the emulated target reboots and reports the reset cause Watchdog)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(WaitForLine(fixture, "Reset Cause: Watchdog", std::chrono::seconds{ 30 }))
        << "emulated target did not reboot reporting a watchdog reset";
    EXPECT_TRUE(fixture.WaitForCanHeartbeat()) << "emulated target did not come back after the watchdog reset";
}

WHEN(R"(the watchdog_stall command is sent to the hardware target)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog_stall", hil::timeouts::command))
        << "watchdog_stall command did not respond";
    ASSERT_TRUE(ContainsLine(fixture, "[WDT] stalling event loop"))
        << "watchdog_stall did not report that the event loop is about to stall";
}
