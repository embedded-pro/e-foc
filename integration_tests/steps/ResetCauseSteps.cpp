#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/TerminalFixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <thread>

using namespace integration;

namespace
{
    const std::string kBootBannerLine{ "Reset Cause:" };

    void ExpectReportedResetCause(TerminalFixture& fixture, const std::string& cause)
    {
        ASSERT_TRUE(fixture.SendCommand("reset_cause", hil::timeouts::command))
            << "reset_cause command did not respond";

        bool found = false;
        for (const auto& line : fixture.allLines)
        {
            if (line.find(cause) != std::string::npos)
            {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Expected '" << cause << "' in reset_cause output";
    }
}

WHEN(R"(the reset command is sent to the hardware target)")
{
    context.Get<TerminalFixture>().SendCommand("reset", hil::timeouts::cleanup);
}

WHEN(R"(the hardware target reconnects after reset)")
{
    auto& fixture = context.Get<TerminalFixture>();

    ASSERT_TRUE(fixture.WaitForRestart(kBootBannerLine, hil::timeouts::bootBanner))
        << "hardware target did not restart: no boot banner and no bridge reconnection";

    const auto deadline = std::chrono::steady_clock::now() + hil::timeouts::bootBanner;
    bool reconnected = false;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (fixture.WaitForPrompt(hil::timeouts::promptShort))
        {
            reconnected = true;
            break;
        }
        std::this_thread::sleep_for(hil::timeouts::reconnectPoll);
    }

    ASSERT_TRUE(reconnected) << "hardware target did not reconnect after reset";
    ASSERT_TRUE(fixture.SendCommand("help", hil::timeouts::prompt))
        << "hardware target reconnected but did not respond to 'help'";
}

THEN(R"(the reset_cause command reports Software)")
{
    ExpectReportedResetCause(context.Get<TerminalFixture>(), "Software");
}

THEN(R"(the reset_cause command reports Watchdog)")
{
    ExpectReportedResetCause(context.Get<TerminalFixture>(), "Watchdog");
}
