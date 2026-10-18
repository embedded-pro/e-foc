#include "core/services/cli/TerminalHelper.hpp"
#include "gmock/gmock.h"

namespace
{
    TEST(TerminalHelperTest, a_whole_number_is_parsed)
    {
        const auto value = services::ParseInput("3");

        ASSERT_TRUE(value.has_value());
        EXPECT_NEAR(*value, 3.0f, 1e-6f);
    }

    TEST(TerminalHelperTest, zero_is_parsed)
    {
        const auto value = services::ParseInput("0");

        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, 0.0f);
    }

    TEST(TerminalHelperTest, a_negative_whole_number_is_parsed)
    {
        const auto value = services::ParseInput("-12");

        ASSERT_TRUE(value.has_value());
        EXPECT_NEAR(*value, -12.0f, 1e-6f);
    }

    TEST(TerminalHelperTest, a_decimal_number_is_parsed)
    {
        const auto value = services::ParseInput("2.5");

        ASSERT_TRUE(value.has_value());
        EXPECT_NEAR(*value, 2.5f, 1e-6f);
    }

    TEST(TerminalHelperTest, text_is_rejected)
    {
        EXPECT_FALSE(services::ParseInput("abc").has_value());
    }

    TEST(TerminalHelperTest, a_value_outside_the_bounds_is_rejected)
    {
        EXPECT_FALSE(services::ParseInput("30", 1.0f, 20.0f).has_value());
        EXPECT_TRUE(services::ParseInput("20", 1.0f, 20.0f).has_value());
    }
}
