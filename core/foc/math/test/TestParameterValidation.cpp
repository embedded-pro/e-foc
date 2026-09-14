#include "core/foc/math/ParameterValidation.hpp"
#include <gmock/gmock.h>
#include <limits>

namespace
{
    class TestParameterValidation
        : public ::testing::Test
    {
    public:
        static float Nan()
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

        static float Inf()
        {
            return std::numeric_limits<float>::infinity();
        }
    };
}

TEST_F(TestParameterValidation, a_value_inside_the_range_is_accepted_including_both_bounds)
{
    EXPECT_TRUE(foc::IsWithinInclusive(1.0f, 1.0f, 10.0f));
    EXPECT_TRUE(foc::IsWithinInclusive(5.0f, 1.0f, 10.0f));
    EXPECT_TRUE(foc::IsWithinInclusive(10.0f, 1.0f, 10.0f));
}

TEST_F(TestParameterValidation, a_value_outside_the_range_is_rejected)
{
    EXPECT_FALSE(foc::IsWithinInclusive(0.999f, 1.0f, 10.0f));
    EXPECT_FALSE(foc::IsWithinInclusive(10.001f, 1.0f, 10.0f));
    EXPECT_FALSE(foc::IsWithinInclusive(-5.0f, 1.0f, 10.0f));
}

TEST_F(TestParameterValidation, a_non_finite_value_is_rejected_rather_than_slipping_through_the_comparisons)
{
    EXPECT_FALSE(foc::IsWithinInclusive(Nan(), 1.0f, 10.0f));
    EXPECT_FALSE(foc::IsWithinInclusive(Inf(), 1.0f, 10.0f));
    EXPECT_FALSE(foc::IsWithinInclusive(-Inf(), 1.0f, 10.0f));
}

TEST_F(TestParameterValidation, positive_infinity_does_not_satisfy_a_positive_domain)
{
    EXPECT_TRUE(foc::IsFinitePositive(1e-9f));
    EXPECT_FALSE(foc::IsFinitePositive(Inf()));
    EXPECT_FALSE(foc::IsFinitePositive(Nan()));
    EXPECT_FALSE(foc::IsFinitePositive(0.0f));
    EXPECT_FALSE(foc::IsFinitePositive(-1.0f));
}

TEST_F(TestParameterValidation, a_non_negative_domain_admits_zero_but_not_infinity)
{
    EXPECT_TRUE(foc::IsFiniteNonNegative(0.0f));
    EXPECT_TRUE(foc::IsFiniteNonNegative(2.5f));
    EXPECT_FALSE(foc::IsFiniteNonNegative(-0.001f));
    EXPECT_FALSE(foc::IsFiniteNonNegative(Inf()));
    EXPECT_FALSE(foc::IsFiniteNonNegative(Nan()));
}

TEST_F(TestParameterValidation, each_loop_bandwidth_carries_its_own_upper_bound)
{
    EXPECT_TRUE(foc::IsAcceptableCurrentBandwidth(foc::CommandLimits::maxCurrentBandwidth));
    EXPECT_FALSE(foc::IsAcceptableCurrentBandwidth(foc::CommandLimits::maxCurrentBandwidth + 1.0f));

    EXPECT_TRUE(foc::IsAcceptableSpeedBandwidth(foc::CommandLimits::maxSpeedBandwidth));
    EXPECT_FALSE(foc::IsAcceptableSpeedBandwidth(foc::CommandLimits::maxSpeedBandwidth + 1.0f));

    EXPECT_TRUE(foc::IsAcceptablePositionBandwidth(foc::CommandLimits::maxPositionBandwidth));
    EXPECT_FALSE(foc::IsAcceptablePositionBandwidth(foc::CommandLimits::maxPositionBandwidth + 1.0f));
}

TEST_F(TestParameterValidation, a_bandwidth_at_or_below_zero_is_rejected_on_every_loop)
{
    EXPECT_FALSE(foc::IsAcceptableCurrentBandwidth(0.0f));
    EXPECT_FALSE(foc::IsAcceptableSpeedBandwidth(0.0f));
    EXPECT_FALSE(foc::IsAcceptablePositionBandwidth(0.0f));

    EXPECT_FALSE(foc::IsAcceptableCurrentBandwidth(-1.0f));
    EXPECT_FALSE(foc::IsAcceptableSpeedBandwidth(-188.5f));
    EXPECT_FALSE(foc::IsAcceptablePositionBandwidth(-18.8f));
}

TEST_F(TestParameterValidation, a_non_finite_bandwidth_is_rejected_on_every_loop)
{
    EXPECT_FALSE(foc::IsAcceptableCurrentBandwidth(Nan()));
    EXPECT_FALSE(foc::IsAcceptableSpeedBandwidth(Nan()));
    EXPECT_FALSE(foc::IsAcceptablePositionBandwidth(Nan()));

    EXPECT_FALSE(foc::IsAcceptableCurrentBandwidth(Inf()));
    EXPECT_FALSE(foc::IsAcceptableSpeedBandwidth(Inf()));
    EXPECT_FALSE(foc::IsAcceptablePositionBandwidth(Inf()));
}
