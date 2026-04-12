#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include <gtest/gtest.h>
#include <sstream>

using namespace hil;

// Known test pattern written to EEPROM address 0 (two bytes: 0xAA 0x55 = 170 85).
static constexpr int testByteA{ 170 };
static constexpr int testByteB{ 55 };

WHEN(R"(a calibration data block is written to EEPROM)")
{
    auto& fixture = context.Get<HilFixture>();
    // Firmware command: 'eeprom_write <addr> <b0> [b1...]'
    ASSERT_TRUE(fixture.SendCommand("eeprom_write 0 170 55"))
        << "eeprom_write command did not receive a response";
}

THEN(R"(the data read back from EEPROM shall be identical to the written data)")
{
    auto& fixture = context.Get<HilFixture>();
    // Firmware command: 'eeprom_read <addr> <size>'
    ASSERT_TRUE(fixture.SendCommand("eeprom_read 0 2"))
        << "eeprom_read command did not receive a response";

    const auto& response = fixture.lastResponse;
    ASSERT_FALSE(response.empty()) << "No EEPROM read response received";

    // The firmware echoes the bytes as decimal integers separated by spaces.
    std::istringstream ss{ response };
    int readA{ -1 }, readB{ -1 };
    ss >> readA >> readB;
    ASSERT_FALSE(ss.fail())
        << "Could not parse two bytes from EEPROM read response: '" << response << "'";
    EXPECT_EQ(readA, testByteA)
        << "First byte mismatch: wrote " << testByteA << ", read " << readA;
    EXPECT_EQ(readB, testByteB)
        << "Second byte mismatch: wrote " << testByteB << ", read " << readB;
}
