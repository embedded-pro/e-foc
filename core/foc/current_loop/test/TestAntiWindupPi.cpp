#include "core/foc/current_loop/AntiWindupPi.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    class TestAntiWindupPi
        : public ::testing::Test
    {
    public:
        foc::AntiWindupPi pi;
    };
}

TEST_F(TestAntiWindupPi, an_unsaturated_proposal_matches_the_proportional_step)
{
    pi.SetTunings(2.0f, 0.0f);

    auto proposed = pi.Propose(5.0f, 0.0f);

    EXPECT_NEAR(10.0f, proposed, tolerance);
}

TEST_F(TestAntiWindupPi, committing_a_realized_value_back_calculates_the_state_for_the_next_proposal)
{
    pi.SetTunings(2.0f, 0.0f);

    pi.Propose(5.0f, 0.0f);
    pi.CommitRealized(6.0f);

    auto next = pi.Propose(5.0f, 0.0f);

    EXPECT_NEAR(6.0f, next, tolerance);
}

TEST_F(TestAntiWindupPi, without_committing_a_realized_value_the_next_proposal_keeps_building_from_the_raw_output)
{
    pi.SetTunings(2.0f, 0.0f);

    pi.Propose(5.0f, 0.0f);

    auto next = pi.Propose(5.0f, 0.0f);

    EXPECT_NEAR(10.0f, next, tolerance);
}

TEST_F(TestAntiWindupPi, reset_discards_both_the_error_history_and_any_committed_realized_value)
{
    pi.SetTunings(2.0f, 0.0f);

    pi.Propose(5.0f, 0.0f);
    pi.CommitRealized(6.0f);
    pi.Reset();

    auto next = pi.Propose(5.0f, 0.0f);

    EXPECT_NEAR(10.0f, next, tolerance);
}
