#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#include "gtest/gtest.h"

namespace
{
    TEST(TestParseIPv4, parse_valid_loopback)
    {
        const auto addr = tool::ParseIPv4("127.0.0.1");
        EXPECT_EQ(addr[0], 127u);
        EXPECT_EQ(addr[1], 0u);
        EXPECT_EQ(addr[2], 0u);
        EXPECT_EQ(addr[3], 1u);
    }

    TEST(TestParseIPv4, parse_valid_all_zeros)
    {
        const auto addr = tool::ParseIPv4("0.0.0.0");
        EXPECT_EQ(addr[0], 0u);
        EXPECT_EQ(addr[1], 0u);
        EXPECT_EQ(addr[2], 0u);
        EXPECT_EQ(addr[3], 0u);
    }

    TEST(TestParseIPv4, parse_valid_broadcast)
    {
        const auto addr = tool::ParseIPv4("255.255.255.255");
        EXPECT_EQ(addr[0], 255u);
        EXPECT_EQ(addr[1], 255u);
        EXPECT_EQ(addr[2], 255u);
        EXPECT_EQ(addr[3], 255u);
    }

    TEST(TestParseIPv4, parse_valid_mixed_octets)
    {
        const auto addr = tool::ParseIPv4("192.168.1.100");
        EXPECT_EQ(addr[0], 192u);
        EXPECT_EQ(addr[1], 168u);
        EXPECT_EQ(addr[2], 1u);
        EXPECT_EQ(addr[3], 100u);
    }

    TEST(TestParseIPv4, throw_on_empty_string)
    {
        EXPECT_THROW(tool::ParseIPv4(""), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_hostname_string)
    {
        EXPECT_THROW(tool::ParseIPv4("localhost"), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_octet_above_255)
    {
        EXPECT_THROW(tool::ParseIPv4("256.0.0.1"), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_second_octet_above_255)
    {
        EXPECT_THROW(tool::ParseIPv4("10.300.0.1"), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_too_few_octets)
    {
        EXPECT_THROW(tool::ParseIPv4("10.0.0"), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_too_many_octets)
    {
        EXPECT_THROW(tool::ParseIPv4("10.0.0.1.2"), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_negative_representation)
    {
        EXPECT_THROW(tool::ParseIPv4("-1.0.0.1"), tool::BridgeArgumentException);
    }

    TEST(TestParseIPv4, throw_on_whitespace_only)
    {
        EXPECT_THROW(tool::ParseIPv4("   "), tool::BridgeArgumentException);
    }
}
