#include "core/foc/math/FrictionFeedForward.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <limits>

namespace
{
    constexpr float torqueConstant{ 0.042f };
    constexpr float coulomb{ 4.2e-3f };
    constexpr float limit{ 10.0f };
    constexpr float amplitude{ coulomb / torqueConstant };

    class TestFrictionFeedForward
        : public ::testing::Test
    {
    public:
        void Load(float friction = coulomb, float kt = torqueConstant, float ceiling = limit)
        {
            feedForward.Load(foc::NewtonMeter{ friction }, foc::NewtonMeter{ kt }, foc::Ampere{ ceiling });
        }

        foc::FrictionFeedForward feedForward;
    };
}

TEST_F(TestFrictionFeedForward, nothing_loaded_adds_no_current)
{
    EXPECT_EQ(feedForward.CurrentFor(100.0f), 0.0f);
    EXPECT_EQ(feedForward.CurrentFor(-100.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, a_reference_beyond_the_ramp_adds_the_friction_current_with_the_reference_sign)
{
    Load();

    EXPECT_NEAR(feedForward.CurrentFor(50.0f), amplitude, 1e-6f);
    EXPECT_NEAR(feedForward.CurrentFor(-50.0f), -amplitude, 1e-6f);
}

TEST_F(TestFrictionFeedForward, a_reference_inside_the_ramp_adds_a_proportional_share)
{
    Load();
    const auto half = 0.5f * foc::FrictionFeedForward::rampWidth;

    EXPECT_NEAR(feedForward.CurrentFor(half), 0.5f * amplitude, 1e-6f);
    EXPECT_NEAR(feedForward.CurrentFor(-half), -0.5f * amplitude, 1e-6f);
    EXPECT_EQ(feedForward.CurrentFor(0.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, the_ramp_ends_at_its_width)
{
    Load();

    EXPECT_NEAR(feedForward.CurrentFor(foc::FrictionFeedForward::rampWidth), amplitude, 1e-6f);
    EXPECT_NEAR(feedForward.CurrentFor(-foc::FrictionFeedForward::rampWidth), -amplitude, 1e-6f);
}

TEST_F(TestFrictionFeedForward, the_current_scales_inversely_with_the_torque_constant)
{
    Load(coulomb, 2.0f * torqueConstant);

    EXPECT_NEAR(feedForward.CurrentFor(50.0f), 0.5f * amplitude, 1e-6f);
}

TEST_F(TestFrictionFeedForward, clearing_removes_the_current)
{
    Load();

    feedForward.Clear();

    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, a_negative_friction_is_refused)
{
    Load(-coulomb);

    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, a_non_finite_friction_is_refused)
{
    Load(std::numeric_limits<float>::quiet_NaN());
    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);

    Load(std::numeric_limits<float>::infinity());
    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, a_torque_constant_that_is_not_positive_refuses_the_friction)
{
    Load(coulomb, 0.0f);
    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);

    Load(coulomb, -torqueConstant);
    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, a_friction_that_needs_the_whole_current_limit_is_refused)
{
    Load(limit * torqueConstant);

    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);
}

TEST_F(TestFrictionFeedForward, a_refused_friction_replaces_one_loaded_before)
{
    Load();

    Load(-coulomb);

    EXPECT_EQ(feedForward.CurrentFor(50.0f), 0.0f);
}
