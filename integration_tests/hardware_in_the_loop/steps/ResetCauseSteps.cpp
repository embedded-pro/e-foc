#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace hil;

WHEN(R"(the reset command is sent to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();
    // Command triggers an immediate reset — no response will arrive
    fixture.SendCommand("reset", std::chrono::milliseconds{ 200 });
}

WHEN(R"(the hardware target reconnects after reset)")
{
    // Wait for the firmware to reboot and re-initialize the UART
    std::this_thread::sleep_for(std::chrono::milliseconds{ 3000 });
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("help", std::chrono::milliseconds{ 1000 }))
        << "hardware target did not reconnect after reset";
}

THEN(R"(the reset_cause command reports Software)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("reset_cause", std::chrono::milliseconds{ 500 }))
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
