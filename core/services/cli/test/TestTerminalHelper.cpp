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

    TEST(TerminalHelperTest, a_whole_number_that_wraps_the_parser_is_rejected)
    {
        EXPECT_FALSE(services::ParseInput("4294967286", -100.0f, 100.0f).has_value());
        EXPECT_FALSE(services::ParseInput("-4294967286", -100.0f, 100.0f).has_value());
    }

    TEST(TerminalHelperTest, a_number_followed_by_other_characters_is_rejected)
    {
        EXPECT_FALSE(services::ParseInput("5abc").has_value());
        EXPECT_FALSE(services::ParseInput("1e3").has_value());
        EXPECT_FALSE(services::ParseInput("2.5x").has_value());
        EXPECT_FALSE(services::ParseInput("2.").has_value());
        EXPECT_FALSE(services::ParseInput("-").has_value());
        EXPECT_FALSE(services::ParseInput("").has_value());
    }

    TEST(TerminalHelperTest, the_longest_accepted_number_is_parsed)
    {
        const auto value = services::ParseInput("-999999999.999999");

        ASSERT_TRUE(value.has_value());
        EXPECT_NEAR(*value, -1.0e9f, 1.0e3f);
        EXPECT_FALSE(services::ParseInput("1000000000").has_value());
        EXPECT_FALSE(services::ParseInput("0.1234567").has_value());
    }

    TEST(TerminalHelperTest, decimal_token_honours_sign_and_fraction_options)
    {
        EXPECT_TRUE(services::IsDecimalToken("12"));
        EXPECT_TRUE(services::IsDecimalToken("-1.5"));
        EXPECT_FALSE(services::IsDecimalToken("-12", false, true));
        EXPECT_FALSE(services::IsDecimalToken("1.5", true, false));
        EXPECT_FALSE(services::IsDecimalToken("+12"));
    }
}
