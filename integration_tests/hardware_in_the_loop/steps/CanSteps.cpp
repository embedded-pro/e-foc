#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <gtest/gtest.h>

using namespace hil;

WHEN(R"(a CAN command is transmitted to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();

    // First start the CAN bus at 500 kbps — firmware command: 'can_start <bitrate>'
    ASSERT_TRUE(fixture.SendCommand("can_start 500000"))
        << "can_start command did not receive a response";

    // Then transmit a frame — firmware command: 'can_send <id> <b0> [b1...]'
    ASSERT_TRUE(fixture.SendCommand("can_send 256 1 2 3", std::chrono::milliseconds{ 100 }))
        << "can_send command did not receive a response within 100 ms";
}

THEN(R"(a CAN response frame shall be received within 10 milliseconds)")
{
    const auto& fixture = context.Get<HilFixture>();
    ASSERT_FALSE(fixture.lastResponse.empty()) << "No CAN response received after can_send";
    // REQ-HIL-006: response must arrive within 10 ms of the can_send command.
    EXPECT_LE(fixture.lastCommandDuration.count(), 10)
        << "CAN response latency " << fixture.lastCommandDuration.count()
        << " ms exceeds the 10 ms budget (REQ-HIL-006)";
}
