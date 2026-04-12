#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <gtest/gtest.h>
#include <sstream>

using namespace hil;

WHEN(R"(the ADC phase current measurement is requested)")
{
    auto& fixture = context.Get<HilFixture>();
    // Firmware command: 'adc <sample_and_hold>' where sample_and_hold is short/medium/long.
    ASSERT_TRUE(fixture.SendCommand("adc short")) << "ADC command did not receive a response";
}

THEN(R"(the measured phase currents shall be within the idle current range)")
{
    const auto& response = context.Get<HilFixture>().lastResponse;
    ASSERT_FALSE(response.empty()) << "No ADC measurement response received";

    // The 'adc short' response contains three raw ADC counts on one line.
    // At idle with no current flowing, all three channels shall read near zero.
    // Accept counts in [0, 4095] (12-bit ADC); refine per board scaling when known.
    std::istringstream ss{ response };
    int ch0{ -1 }, ch1{ -1 }, ch2{ -1 };
    ss >> ch0 >> ch1 >> ch2;
    ASSERT_FALSE(ss.fail()) << "Could not parse three ADC counts from response: '" << response << "'";
    constexpr int maxAdcCount = 4095;
    EXPECT_GE(ch0, 0) << "Phase A ADC count is negative";
    EXPECT_LE(ch0, maxAdcCount) << "Phase A ADC count exceeds 12-bit range";
    EXPECT_GE(ch1, 0) << "Phase B ADC count is negative";
    EXPECT_LE(ch1, maxAdcCount) << "Phase B ADC count exceeds 12-bit range";
    EXPECT_GE(ch2, 0) << "Phase C ADC count is negative";
    EXPECT_LE(ch2, maxAdcCount) << "Phase C ADC count exceeds 12-bit range";
}
