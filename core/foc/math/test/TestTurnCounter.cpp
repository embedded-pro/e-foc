#include "core/foc/math/TurnCounter.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float turn = 2.0f * pi;
}

TEST(TurnCounter, starts_unprimed_at_turn_zero)
{
    foc::TurnCounter counter;

    EXPECT_FALSE(counter.IsPrimed());
    EXPECT_EQ(0, counter.Count().turns);
}

TEST(TurnCounter, the_first_sample_sets_the_angle_and_counts_nothing)
{
    foc::TurnCounter counter;

    counter.Track(3.0f);

    EXPECT_TRUE(counter.IsPrimed());
    EXPECT_EQ(0, counter.Count().turns);
    EXPECT_FLOAT_EQ(3.0f, counter.Count().angle);
}

TEST(TurnCounter, a_forward_crossing_of_the_seam_adds_a_turn)
{
    foc::TurnCounter counter;

    counter.Track(3.0f);
    counter.Track(-3.0f);

    EXPECT_EQ(1, counter.Count().turns);
    EXPECT_NEAR(turn - 3.0f, foc::PositionOf(counter.Count()), 1e-5f);
}

TEST(TurnCounter, a_backward_crossing_of_the_seam_takes_a_turn_away)
{
    foc::TurnCounter counter;

    counter.Track(-3.0f);
    counter.Track(3.0f);

    EXPECT_EQ(-1, counter.Count().turns);
    EXPECT_NEAR(3.0f - turn, foc::PositionOf(counter.Count()), 1e-5f);
}

TEST(TurnCounter, a_step_within_half_a_turn_keeps_the_turn)
{
    foc::TurnCounter counter;

    counter.Track(-1.5f);
    counter.Track(1.5f);
    counter.Track(-1.5f);

    EXPECT_EQ(0, counter.Count().turns);
    EXPECT_FLOAT_EQ(-1.5f, counter.Count().angle);
}

TEST(TurnCounter, counts_three_forward_turns_sampled_a_quarter_turn_apart)
{
    foc::TurnCounter counter;

    for (int step = 0; step <= 12; ++step)
        counter.Track(std::remainder(static_cast<float>(step) * turn / 4.0f, turn));

    EXPECT_EQ(3, counter.Count().turns);
    EXPECT_NEAR(3.0f * turn, foc::PositionOf(counter.Count()), 1e-4f);
}

TEST(TurnCounter, a_seed_continues_from_its_turns_and_unwraps_against_its_angle)
{
    foc::TurnCounter counter;

    counter.Seed(foc::TurnCount{ 7, 3.1f });
    counter.Track(-3.1f);

    EXPECT_EQ(8, counter.Count().turns);
}

TEST(TurnCounter, a_restart_returns_to_turn_zero_and_waits_for_a_sample)
{
    foc::TurnCounter counter;

    counter.Seed(foc::TurnCount{ 4, 1.0f });
    counter.Restart();

    EXPECT_FALSE(counter.IsPrimed());
    EXPECT_EQ(0, counter.Count().turns);

    counter.Track(-3.0f);

    EXPECT_EQ(0, counter.Count().turns);
    EXPECT_FLOAT_EQ(-3.0f, counter.Count().angle);
}

TEST(TurnCounter, the_difference_of_two_counts_keeps_the_resolution_of_the_angle)
{
    const foc::TurnCount far{ 1000, 0.0015f };
    const foc::TurnCount near{ 1000, 0.0005f };

    EXPECT_NEAR(0.001f, foc::Difference(far, near), 1e-7f);
    EXPECT_NEAR(turn + 0.001f, foc::Difference(foc::TurnCount{ 1001, 0.0015f }, near), 1e-5f);
}
