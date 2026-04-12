#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <gtest/gtest.h>

using namespace hil;

WHEN(R"(the PWM three-channel output is started at the default frequency)")
{
    auto& fixture = context.Get<HilFixture>();
    // Firmware command: 'pwm <dead_time_ns> <frequency_hz>'
    // 500 ns dead-time, 20 kHz carrier — the motor control default.
    ASSERT_TRUE(fixture.SendCommand("pwm 500 20000")) << "PWM configure command did not receive a response";
}

THEN(R"(all three PWM channels shall be switching at the configured carrier frequency)")
{
    const auto& response = context.Get<HilFixture>().lastResponse;
    ASSERT_FALSE(response.empty()) << "No PWM status response received";
    // The firmware echoes back the configured parameters upon success.
    // Expect the carrier frequency '20000' to appear in the response.
    EXPECT_NE(response.find("20000"), std::string::npos)
        << "PWM response does not confirm 20000 Hz frequency: '" << response << "'";
}
