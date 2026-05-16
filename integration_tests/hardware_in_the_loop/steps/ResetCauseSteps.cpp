#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace hil;

WHEN(R"(the reset command is sent to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();
    fixture.SendCommand("reset", timeouts::cleanup);
}

WHEN(R"(the hardware target reconnects after reset)")
{
    auto& fixture = context.Get<HilFixture>();

    const auto deadline = std::chrono::steady_clock::now() + timeouts::bootBanner;
    bool reconnected = false;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (fixture.WaitForPrompt(timeouts::promptShort))
        {
            reconnected = true;
            break;
        }
        std::this_thread::sleep_for(timeouts::reconnectPoll);
    }

    ASSERT_TRUE(reconnected)
        << "hardware target did not reconnect after reset";

    ASSERT_TRUE(fixture.SendCommand("help", timeouts::prompt))
        << "hardware target reconnected but did not respond to 'help'";
}

THEN(R"(the reset_cause command reports Software)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("reset_cause", timeouts::command))
        << "reset_cause command did not respond";
    bool foundSoftware = false;
    for (const auto& line : fixture.allLines)
    {
        if (line.find("Software") != std::string::npos)
        {
            foundSoftware = true;
            break;
        }
    }
    EXPECT_TRUE(foundSoftware) << "Expected 'Software' in reset_cause output";
}
