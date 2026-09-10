#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <gtest/gtest.h>

using namespace integration;

WHEN(R"(the force_hardfault command is sent to the hardware target)")
{
    context.Get<Fixture>().SendCommand("force_hardfault", hil::timeouts::cleanup);
}

THEN(R"(the fault_status command reports captured fault data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("fault_status", hil::timeouts::prompt))
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
