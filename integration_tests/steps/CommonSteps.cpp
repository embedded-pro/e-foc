#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the hardware target is connected and responding)")
{
    auto& fixture = context.Get<Fixture>();

    (void)fixture.WaitForPrompt();
    bool responded = fixture.SendCommand("enc");
    if (!responded || fixture.lastResponse.empty())
    {
        (void)fixture.WaitForPrompt();
        responded = fixture.SendCommand("enc");
    }
    ASSERT_TRUE(responded) << "Hardware target did not respond to 'enc' within timeout";
    ASSERT_FALSE(fixture.lastResponse.empty()) << "Encoder liveness probe returned empty response";
}
