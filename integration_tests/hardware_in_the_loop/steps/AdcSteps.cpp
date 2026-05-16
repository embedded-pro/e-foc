#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace hil;

WHEN(R"(the ADC phase current measurement is requested)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("duty 50 50 50", timeouts::command))
        << "failed to start PWM as ADC trigger";
    ASSERT_TRUE(fixture.SendCommand("adc shortest", timeouts::asyncCommand))
        << "ADC command did not receive a response";
    fixture.FlushPartialLines(timeouts::asyncCommand);
}

THEN(R"(the measured phase currents shall be within the idle current range)")
{
    const auto& lines = context.Get<HilFixture>().allLines;
    ASSERT_FALSE(lines.empty()) << "No ADC measurement response received";

    const auto sampleIt = std::find_if(lines.begin(), lines.end(),
        [](const std::string& line)
        {
            const auto first = line.find(';');
            const auto second = first == std::string::npos
                                    ? std::string::npos
                                    : line.find(';', first + 1);
            return first != std::string::npos && second != std::string::npos;
        });

    ASSERT_NE(sampleIt, lines.end())
        << "Could not find any 'a;b;c' phase-current sample line in ADC output";

    std::string row = *sampleIt;
    std::replace(row.begin(), row.end(), ';', ' ');
    std::istringstream ss{ row };
    float a{ 0.0f }, b{ 0.0f }, c{ 0.0f };
    ss >> a >> b >> c;
    ASSERT_FALSE(ss.fail())
        << "Could not parse three floats from phase-current row: '" << *sampleIt << "'";

    constexpr float biasMidscale = 2048.0f;
    constexpr float maxIdleDeviation = 500.0f;
    EXPECT_TRUE(std::isfinite(a) && std::isfinite(b) && std::isfinite(c))
        << "Phase current sample contains non-finite value";
    EXPECT_LE(std::abs(a - biasMidscale), maxIdleDeviation) << "Phase A current outside idle range";
    EXPECT_LE(std::abs(b - biasMidscale), maxIdleDeviation) << "Phase B current outside idle range";
    EXPECT_LE(std::abs(c - biasMidscale), maxIdleDeviation) << "Phase C current outside idle range";
}
