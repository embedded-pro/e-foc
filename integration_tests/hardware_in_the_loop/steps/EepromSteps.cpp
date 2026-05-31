#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace hil;

static constexpr int testByteA{ 170 };
static constexpr int testByteB{ 55 };

namespace
{
    std::optional<std::pair<int, int>> ParseEepromByteLine(const std::string& line)
    {
        const auto open = line.find('[');
        const auto close = line.find(']', open == std::string::npos ? 0 : open);
        const auto eq = line.find('=', close == std::string::npos ? 0 : close);
        if (open == std::string::npos || close == std::string::npos || eq == std::string::npos)
            return std::nullopt;

        try
        {
            const int index = std::stoi(line.substr(open + 1, close - open - 1));
            const int value = std::stoi(line.substr(eq + 1));
            return std::make_pair(index, value);
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }
}

WHEN(R"(a calibration data block is written to EEPROM)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("eeprom_write 0 170 55", timeouts::slowCommand))
        << "eeprom_write command did not receive a response";
}

THEN(R"(the data read back from EEPROM shall be identical to the written data)")
{
    auto& fixture = context.Get<HilFixture>();
    ASSERT_TRUE(fixture.SendCommand("eeprom_read 0 2", timeouts::slowCommand))
        << "eeprom_read command did not receive a response";

    int readA{ -1 }, readB{ -1 };
    for (const auto& line : fixture.allLines)
    {
        const auto parsed = ParseEepromByteLine(line);
        if (!parsed)
            continue;
        if (parsed->first == 0)
            readA = parsed->second;
        else if (parsed->first == 1)
            readB = parsed->second;
    }

    ASSERT_NE(readA, -1) << "EEPROM byte [0] not found in response lines";
    ASSERT_NE(readB, -1) << "EEPROM byte [1] not found in response lines";

    EXPECT_EQ(readA, testByteA)
        << "First byte mismatch: wrote " << testByteA << ", read " << readA;
    EXPECT_EQ(readB, testByteB)
        << "Second byte mismatch: wrote " << testByteB << ", read " << readB;
}
