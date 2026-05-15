#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <string>

using namespace hil;

WHEN(R"(the hardware target is rebooted and emits its banner over UART)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.DrainLines(std::chrono::milliseconds{ 3000 }))
        << "No UART output captured while waiting for the post-reset banner";
}

THEN(R"(the CLI banner shall be well-formed)")
{
    const auto& lines = context.Get<HilFixture>().allLines;
    ASSERT_FALSE(lines.empty()) << "No UART lines captured for banner inspection";

    const auto contains = [&](const std::string& needle)
    {
        for (const auto& line : lines)
            if (line.find(needle) != std::string::npos)
                return true;
        return false;
    };

    EXPECT_TRUE(contains("e-foc:")) << "Banner missing 'e-foc:' target line";
    EXPECT_TRUE(contains("System Clock:")) << "Banner missing 'System Clock:' line";
    EXPECT_TRUE(contains("Power Supply Voltage:")) << "Banner missing 'Power Supply Voltage:' line";
    EXPECT_TRUE(contains("Reset Cause:")) << "Banner missing 'Reset Cause:' line";
}
