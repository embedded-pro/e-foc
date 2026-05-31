#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <gtest/gtest.h>

using namespace hil;

WHEN(R"(the force_hardfault command is sent to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();
    fixture.SendCommand("force_hardfault", timeouts::cleanup);
}

THEN(R"(the fault_status command reports captured fault data)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("fault_status", timeouts::prompt))
        << "fault_status command did not respond";
    bool foundFaultData = false;
    for (const auto& line : fixture.allLines)
    {
        if (line.find("PC  :") != std::string::npos || line.find("CFSR :") != std::string::npos)
        {
            foundFaultData = true;
            break;
        }
    }
    EXPECT_TRUE(foundFaultData)
        << "Expected fault diagnostic data (PC  : or CFSR :) in fault_status output";
}
