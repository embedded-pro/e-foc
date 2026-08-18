#include "core/foc/math/AngleWrap.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float tolerance = 1e-4f;

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

TEST_F(TestAngleWrap, an_angle_above_pi_wraps_down_to_the_short_path)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(pi + 0.5f), -pi + 0.5f, tolerance);
}

TEST_F(TestAngleWrap, an_angle_below_minus_pi_wraps_up_to_the_short_path)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(-pi - 0.5f), pi - 0.5f, tolerance);
}

TEST_F(TestAngleWrap, just_past_the_positive_boundary_lands_just_past_the_negative_one)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(pi + 0.001f), -pi + 0.001f, tolerance);
}

TEST_F(TestAngleWrap, a_large_positive_angle_is_wrapped_to_the_principal_range)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(3.0f * pi), -pi, tolerance);
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(5.0f * pi), -pi, tolerance);
    // 10 rad = 3*2π - 8.849... → round(10/2π)=2 → 10 - 4π ≈ -2.566 rad
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(10.0f), 10.0f - 4.0f * pi, tolerance);
}

TEST_F(TestAngleWrap, a_large_negative_angle_is_wrapped_to_the_principal_range)
{
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(-3.0f * pi), pi, tolerance);
    EXPECT_NEAR(foc::detail::PositionWithWrapAround(-5.0f * pi), pi, tolerance);
}

TEST_F(TestAngleWrap, an_angle_of_100_radians_wraps_to_the_principal_range)
{
    const float wrapped = foc::detail::PositionWithWrapAround(100.0f);
    EXPECT_GE(wrapped, -pi);
    EXPECT_LE(wrapped, pi);
}
