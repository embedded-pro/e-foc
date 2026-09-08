#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace integration;

WHEN(R"(the reset command is sent to the hardware target)")
{
    context.Get<Fixture>().SendCommand("reset", hil::timeouts::cleanup);
}

WHEN(R"(the hardware target reconnects after reset)")
{
    auto& fixture = context.Get<Fixture>();

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
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("reset_cause", hil::timeouts::command))
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
