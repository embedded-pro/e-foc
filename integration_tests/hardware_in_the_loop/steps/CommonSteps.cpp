#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <gtest/gtest.h>

using namespace hil;

GIVEN(R"(the hardware target is connected and responding)")
{
    context.Emplace<HilFixture>();
    auto& fixture = context.Get<HilFixture>();
    // Use 'enc' as a liveness probe — the firmware has no 'ping' command.
    // 'enc' reads encoder position and always produces a single-line response.
    const bool responded = fixture.SendCommand("enc");
    ASSERT_TRUE(responded) << "Hardware target did not respond to 'enc' within timeout";
    ASSERT_FALSE(fixture.lastResponse.empty()) << "Encoder liveness probe returned empty response";
}
