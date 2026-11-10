#include "tools/virtual_motor/engine/StartOptions.hpp"
#include "gtest/gtest.h"
#include <string>
#include <vector>

namespace
{
    virtual_motor::ParsedOptions Parse(std::vector<std::string> arguments)
    {
        return virtual_motor::ParseStartOptions(arguments);
    }
}

TEST(TestStartOptions, defaults_to_node_zero_on_the_default_bus_with_the_teknic_plant)
{
    const auto parsed = Parse({});

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(parsed.options->bus, "default");
    EXPECT_EQ(parsed.options->node, 0);
    EXPECT_EQ(parsed.options->plant, "teknic");
    EXPECT_EQ(parsed.options->memoryFile, "virtual-motor-default-0.nvm");
    EXPECT_EQ(parsed.options->hubPort, 29536);
    EXPECT_FALSE(parsed.options->factoryFresh);
    EXPECT_FALSE(parsed.options->hostBus);
    EXPECT_FALSE(parsed.options->restarted);
    EXPECT_FALSE(parsed.options->seed.has_value());
}

TEST(TestStartOptions, reads_every_option)
{
    const auto parsed = Parse({ "--bus", "bench", "--node", "3", "--plant", "anaheim", "--memory", "m.nvm", "--seed", "42", "--hub-port", "4000", "--factory-fresh", "--host-bus", "--restarted" });

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(parsed.options->bus, "bench");
    EXPECT_EQ(parsed.options->node, 3);
    EXPECT_EQ(parsed.options->plant, "anaheim");
    EXPECT_EQ(parsed.options->memoryFile, "m.nvm");
    EXPECT_EQ(parsed.options->seed, 42u);
    EXPECT_EQ(parsed.options->hubPort, 4000);
    EXPECT_TRUE(parsed.options->factoryFresh);
    EXPECT_TRUE(parsed.options->hostBus);
    EXPECT_TRUE(parsed.options->restarted);
}

TEST(TestStartOptions, names_the_memory_file_after_the_bus_and_node)
{
    const auto parsed = Parse({ "--bus", "bench", "--node", "7" });

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(parsed.options->memoryFile, "virtual-motor-bench-7.nvm");
}

TEST(TestStartOptions, refuses_an_unknown_option)
{
    const auto parsed = Parse({ "--speed", "2" });

    EXPECT_FALSE(parsed.options.has_value());
    EXPECT_EQ(parsed.error, "unknown option --speed");
}

TEST(TestStartOptions, refuses_an_option_without_its_value)
{
    const auto parsed = Parse({ "--bus" });

    EXPECT_FALSE(parsed.options.has_value());
    EXPECT_EQ(parsed.error, "--bus needs a value");
}

TEST(TestStartOptions, refuses_a_node_or_port_out_of_range)
{
    EXPECT_EQ(Parse({ "--node", "256" }).error, "--node must be a number from 0 to 255");
    EXPECT_EQ(Parse({ "--node", "x" }).error, "--node must be a number from 0 to 255");
    EXPECT_EQ(Parse({ "--hub-port", "70000" }).error, "--hub-port must be a number from 1 to 65535");
}

TEST(TestStartOptions, refuses_a_bus_name_a_join_line_cannot_carry)
{
    EXPECT_EQ(Parse({ "--bus", "a-bus-name-longer-than-twenty-four" }).error, "--bus must be 1 to 24 characters");
}

TEST(TestStartOptions, help_is_asked_for_not_an_error)
{
    const auto parsed = Parse({ "--help" });

    EXPECT_FALSE(parsed.options.has_value());
    EXPECT_TRUE(parsed.help);
    EXPECT_TRUE(parsed.error.empty());
}
