#include "core/foc/cascade/TurnHandover.hpp"
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    constexpr float turn = 2.0f * std::numbers::pi_v<float>;

    class TurnHandoverTest
        : public ::testing::Test
    {
    public:
        void TurnForwardTwice()
        {
            for (const float angle : { 0.0f, 2.0f, -2.0f, 0.0f, 2.0f, -2.0f, 0.0f })
                handover.Track(angle);
        }

        foc::TurnHandover handover;
    };
}

TEST_F(TurnHandoverTest, reports_turn_zero_before_any_sample)
{
    EXPECT_FLOAT_EQ(0.0f, handover.Position().Value());
}

TEST_F(TurnHandoverTest, counts_the_turns_of_its_samples)
{
    TurnForwardTwice();

    EXPECT_NEAR(2.0f * turn, handover.Position().Value(), 1e-4f);
}

TEST_F(TurnHandoverTest, a_hold_hands_over_the_count_and_the_angle_it_was_taken_at)
{
    TurnForwardTwice();
    handover.Track(1.0f);

    const auto held = handover.Hold();

    ASSERT_TRUE(held.has_value());
    EXPECT_EQ(2, held->turns);
    EXPECT_FLOAT_EQ(1.0f, held->angle);
}

TEST_F(TurnHandoverTest, there_is_nothing_to_hold_before_the_first_sample)
{
    EXPECT_FALSE(handover.Hold().has_value());
}

TEST_F(TurnHandoverTest, while_held_its_own_samples_are_ignored_and_the_published_count_is_reported)
{
    TurnForwardTwice();
    handover.Hold();

    handover.Track(3.0f);
    handover.Track(-3.0f);
    handover.Publish(foc::TurnCount{ 5, 0.5f });

    EXPECT_NEAR(5.0f * turn + 0.5f, handover.Position().Value(), 1e-4f);
}

TEST_F(TurnHandoverTest, while_held_and_nothing_published_the_held_count_is_reported)
{
    TurnForwardTwice();
    handover.Hold();

    EXPECT_NEAR(2.0f * turn, handover.Position().Value(), 1e-4f);
}

TEST_F(TurnHandoverTest, a_release_counts_on_from_the_last_published_count)
{
    handover.Track(0.0f);
    handover.Hold();
    handover.Publish(foc::TurnCount{ 3, 3.0f });
    handover.Release();

    handover.Track(-3.0f);

    EXPECT_NEAR(4.0f * turn - 3.0f, handover.Position().Value(), 1e-4f);
}

TEST_F(TurnHandoverTest, a_restart_returns_to_turn_zero_and_takes_the_next_sample_as_its_angle)
{
    TurnForwardTwice();

    handover.Restart();
    handover.Track(-3.0f);

    EXPECT_FLOAT_EQ(-3.0f, handover.Position().Value());
}

TEST_F(TurnHandoverTest, a_count_released_after_a_restart_is_discarded)
{
    TurnForwardTwice();
    handover.Hold();
    handover.Publish(foc::TurnCount{ 7, 1.0f });

    handover.Restart();
    handover.Release();
    handover.Track(1.0f);

    EXPECT_FLOAT_EQ(1.0f, handover.Position().Value());
}
