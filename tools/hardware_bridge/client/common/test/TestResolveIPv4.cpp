#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include "tools/hardware_bridge/client/common/ResolveIPv4.hpp"
#include <gtest/gtest.h>

TEST(TestResolveIPv4, dotted_quad_is_returned_without_lookup)
{
    const services::IPv4Address expected{ 192, 168, 1, 10 };
    EXPECT_EQ(tool::ResolveIPv4("192.168.1.10"), expected);
}

TEST(TestResolveIPv4, hostname_is_resolved)
{
    const services::IPv4Address expected{ 127, 0, 0, 1 };
    EXPECT_EQ(tool::ResolveIPv4("localhost"), expected);
}

TEST(TestResolveIPv4, unresolvable_host_throws_argument_exception)
{
    EXPECT_THROW(tool::ResolveIPv4(""), tool::BridgeArgumentException);
}
