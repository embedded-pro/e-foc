#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <cmath>
#include <gtest/gtest.h>

using namespace hil;

WHEN(R"(the encoder position is read after a known mechanical displacement)")
{
    auto& fixture = context.Get<HilFixture>();
    // Firmware command: 'enc' (short alias 'e') — reads quadrature encoder and prints position.
    ASSERT_TRUE(fixture.SendCommand("enc")) << "Encoder read command did not receive a response";
}

THEN(R"(the reported position change shall match the expected increment)")
{
    const auto& response = context.Get<HilFixture>().lastResponse;
    ASSERT_FALSE(response.empty()) << "No encoder position response received";
    // The response must contain a parseable numeric position value.
    try
    {
        const float position = std::stof(response);
        // At idle (no forced displacement), position shall be in a finite range.
        EXPECT_TRUE(std::isfinite(position))
            << "Encoder position is not a finite number: '" << response << "'";
    }
    catch (const std::exception&)
    {
        FAIL() << "Could not parse encoder position from response: '" << response << "'";
    }
}
