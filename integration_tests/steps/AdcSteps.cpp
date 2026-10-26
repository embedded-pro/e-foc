#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/TerminalFixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <regex>
#include <string>
#include <vector>

using namespace integration;

namespace
{
    std::optional<std::array<float, 3>> PhaseCurrentRow(const std::string& line)
    {
        static const std::regex row{ R"((?:^|\s)(-?\d+\.\d+);(-?\d+\.\d+);(-?\d+\.\d+)\s*$)" };

        std::smatch match;
        if (!std::regex_search(line, match, row))
            return std::nullopt;

        return std::array<float, 3>{ std::stof(match[1].str()), std::stof(match[2].str()), std::stof(match[3].str()) };
    }
}

WHEN(R"(the ADC phase current measurement is requested)")
{
    auto& fixture = context.Get<TerminalFixture>();
    ASSERT_TRUE(fixture.SendCommand("adc shortest", hil::timeouts::command))
        << "ADC command did not receive a response";
    ASSERT_TRUE(fixture.SendCommand("duty 50 50 50", hil::timeouts::asyncCommand))
        << "failed to start PWM as ADC trigger";
    fixture.FlushPartialLines(hil::timeouts::asyncCommand);
}

THEN(R"(the measured phase currents shall be within the idle current range)")
{
    const auto& lines = context.Get<TerminalFixture>().allLines;
    ASSERT_FALSE(lines.empty()) << "No ADC measurement response received";

    std::vector<std::array<float, 3>> samples;
    for (const auto& line : lines)
        if (const auto sample = PhaseCurrentRow(line))
            samples.push_back(*sample);

    ASSERT_FALSE(samples.empty()) << "Could not find any 'a;b;c' phase-current sample row in ADC output";

    for (const auto& sample : samples)
        EXPECT_TRUE(std::isfinite(sample[0]) && std::isfinite(sample[1]) && std::isfinite(sample[2]))
            << "Phase current sample contains non-finite value";
}
