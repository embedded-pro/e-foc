#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <gtest/gtest.h>

using namespace hil;

WHEN(R"(a CAN command is transmitted to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();

    ASSERT_TRUE(fixture.SendCommand("can_start 500000"))
        << "can_start command did not receive a response";

    ASSERT_TRUE(fixture.SendCommand("can_send 256 1 2 3", std::chrono::milliseconds{ 100 }))
        << "can_send command did not receive a response within 100 ms";
}

THEN(R"(a CAN response frame shall be received within 10 milliseconds)")
{
    const auto& fixture = context.Get<HilFixture>();
    ASSERT_FALSE(fixture.lastResponse.empty()) << "No CAN response received after can_send";
    EXPECT_LE(fixture.lastCommandDuration.count(), 10)
        << "CAN response latency " << fixture.lastCommandDuration.count()
        << " ms exceeds the 10 ms budget (REQ-HIL-006)";
}
