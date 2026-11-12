#include "core/services/mechanical_system_ident/InertiaSettling.hpp"
#include "gtest/gtest.h"

namespace
{
    using services::InertiaSettling;

    class TestInertiaSettling
        : public testing::Test
    {
    public:
        void Track(float inertia, int updates)
        {
            for (int update = 0; update != updates; ++update)
                settling.Track(inertia, 100);
        }

        InertiaSettling settling;
    };
}

TEST_F(TestInertiaSettling, the_first_comparison_only_sets_the_reference)
{
    settling.Restart(0.99f);
    Track(1.0e-4f, 8);

    EXPECT_FALSE(settling.Settled());
    EXPECT_NEAR(0.99f, settling.ForgettingFactor(), 1e-6f);
}

TEST_F(TestInertiaSettling, an_unchanged_inertia_one_memory_later_has_settled)
{
    settling.Restart(0.99f);
    Track(1.0e-4f, 8);
    Track(1.05e-4f, 8);

    EXPECT_TRUE(settling.Settled());
}

TEST_F(TestInertiaSettling, an_inertia_that_moved_by_more_than_a_tenth_has_not_settled)
{
    settling.Restart(0.99f);
    Track(1.0e-4f, 8);
    Track(1.5e-4f, 8);

    EXPECT_FALSE(settling.Settled());
}

TEST_F(TestInertiaSettling, without_forgetting_the_memory_spans_two_thousand_samples)
{
    settling.Restart(1.0f);
    Track(1.0e-4f, 20);
    Track(1.0e-4f, 19);
    EXPECT_FALSE(settling.Settled());

    Track(1.0e-4f, 1);
    EXPECT_TRUE(settling.Settled());
}

TEST_F(TestInertiaSettling, restarting_forgets_the_reference)
{
    settling.Restart(0.99f);
    Track(1.0e-4f, 16);
    settling.Restart(0.99f);
    Track(1.0e-4f, 8);

    EXPECT_FALSE(settling.Settled());
}
