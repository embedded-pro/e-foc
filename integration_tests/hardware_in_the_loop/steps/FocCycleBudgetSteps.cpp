#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

using namespace hil;

// 75% of one control period at 120 MHz / 20 kHz = 0.75 * (120e6 / 20e3) = 4500 cycles.
static constexpr uint32_t kMaxAllowedCycles{ 4500 };

WHEN(R"(the FOC loop CPU utilisation is sampled for one control cycle)")
{
    auto& fixture = context.Get<HilFixture>();
    // Firmware command: 'foc <angle_deg> <ia> <ib> <ic>' — runs one FOC calculation
    // and reports execution time in CPU cycles on a line: '      Execution time:   N cycles'
    ASSERT_TRUE(fixture.SendCommand("foc 0 0 0 0", std::chrono::milliseconds{ 500 }))
        << "FOC simulation command did not receive a response";
}

THEN(R"(the measured utilisation shall not exceed 75 percent of one control period)")
{
    const auto& lines = context.Get<HilFixture>().allLines;
    ASSERT_FALSE(lines.empty()) << "No response lines received from FOC simulation command";

    // Find the line containing 'Execution time:'
    const auto it = std::find_if(lines.begin(), lines.end(),
        [](const std::string& line)
        {
            return line.find("Execution time:") != std::string::npos;
        });

    ASSERT_NE(it, lines.end())
        << "Could not find 'Execution time:' in FOC simulation output";

    // Parse the cycle count: '      Execution time:   N cycles'
    const auto colonPos = it->find(':');
    ASSERT_NE(colonPos, std::string::npos);
    const std::string countStr = it->substr(colonPos + 1);
    uint32_t cycles{ 0 };
    try
    {
        cycles = static_cast<uint32_t>(std::stoul(countStr));
    }
    catch (const std::exception&)
    {
        FAIL() << "Could not parse cycle count from line: '" << *it << "'";
    }

    EXPECT_LE(cycles, kMaxAllowedCycles)
        << "FOC loop used " << cycles << " cycles, exceeding the 75% budget of "
        << kMaxAllowedCycles << " cycles at 120 MHz / 20 kHz";
}
