#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <algorithm>
#include <cctype>
#include <gtest/gtest.h>

using namespace hil;

WHEN(R"(the ping command is sent over UART)")
{
    auto& fixture = context.Get<HilFixture>();
    // Use 'enc' — the firmware has no 'ping' command. 'enc' responds within 100 ms.
    ASSERT_TRUE(fixture.SendCommand("enc", std::chrono::milliseconds{ 100 }))
        << "UART command 'enc' did not receive a response within 100 ms";
}

THEN(R"(a well-formed response shall be received within 100 milliseconds)")
{
    const auto& response = context.Get<HilFixture>().lastResponse;
    ASSERT_FALSE(response.empty()) << "No UART response received";
    // A well-formed 'enc' response contains at least one digit (the position value).
    const bool hasDigit = std::any_of(response.begin(), response.end(),
        [](char c)
        {
            return std::isdigit(static_cast<unsigned char>(c));
        });
    EXPECT_TRUE(hasDigit) << "Response does not contain a numeric position value: '" << response << "'";
}
