#include "core/foc/math/AngleWrap.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float tolerance = 1e-5f;

    class TestAngleWrap
        : public ::testing::Test
    {
    };
}

TEST_F(TestAngleWrap, zero_is_left_untouched)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(0.0f), 0.0f, tolerance);
}

TEST_F(TestAngleWrap, an_angle_inside_the_principal_range_is_left_untouched)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(1.0f), 1.0f, tolerance);
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(-1.0f), -1.0f, tolerance);
}

TEST_F(TestAngleWrap, an_angle_above_pi_wraps_down_by_one_turn)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(pi + 0.5f), -pi + 0.5f, tolerance);
}

TEST_F(TestAngleWrap, an_angle_below_minus_pi_wraps_up_by_one_turn)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(-pi - 0.5f), pi - 0.5f, tolerance);
}

TEST_F(TestAngleWrap, the_positive_boundary_itself_is_not_wrapped)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(pi), pi, tolerance);
}

TEST_F(TestAngleWrap, the_negative_boundary_itself_is_not_wrapped)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(-pi), -pi, tolerance);
}

TEST_F(TestAngleWrap, just_past_the_positive_boundary_lands_just_past_the_negative_one)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(pi + 0.001f), -pi + 0.001f, tolerance);
}

TEST_F(TestAngleWrap, a_single_turn_at_most_is_removed_per_call)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(3.0f * pi), pi, tolerance);
}
