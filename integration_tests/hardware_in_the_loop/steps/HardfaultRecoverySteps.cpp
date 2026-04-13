#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace hil;

WHEN(R"(the force_hardfault command is sent to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();
    // Command causes a crash — ignore the return; no response will arrive
    fixture.SendCommand("force_hardfault", std::chrono::milliseconds{ 200 });
}

THEN(R"(the fault_status command reports captured fault data)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("fault_status", std::chrono::milliseconds{ 500 }))
        << "fault_status command did not respond";
    bool foundFaultData = false;
    for (const auto& line : fixture.allLines)
    {
        if (line.find("PC=") != std::string::npos || line.find("CFSR=") != std::string::npos)
        {
            foundFaultData = true;
            break;
        }
    }
    EXPECT_TRUE(foundFaultData)
        << "Expected fault diagnostic data (PC= or CFSR=) in fault_status output";
}
