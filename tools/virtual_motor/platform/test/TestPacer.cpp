#include "tools/virtual_motor/platform/Pacer.hpp"
#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace std::chrono_literals;

    class TestPacer
        : public testing::Test
    {
    public:
        virtual_motor::Pacer::TimePoint now{};
        uint32_t steps{ 0 };
        virtual_motor::Pacer pacer{ [this]()
            {
                return now;
            },
            [this]()
            {
                ++steps;
            } };
    };
}

TEST_F(TestPacer, runs_one_step_per_elapsed_millisecond)
{
    now += 3ms;
    pacer.Service();
    EXPECT_EQ(steps, 3u);

    now += 500us;
    pacer.Service();
    EXPECT_EQ(steps, 3u);

    now += 500us;
    pacer.Service();
    EXPECT_EQ(steps, 4u);
}

TEST_F(TestPacer, the_next_step_is_due_one_millisecond_after_the_last)
{
    now += 2ms;
    pacer.Service();

    EXPECT_EQ(pacer.NextDue(), virtual_motor::Pacer::TimePoint{ 3ms });
}

TEST_F(TestPacer, a_backlog_beyond_the_catch_up_limit_is_dropped_and_counted)
{
    now += 200ms;
    pacer.Service();

    EXPECT_EQ(steps, 0u);
    EXPECT_EQ(pacer.Overruns(), 1u);

    now += 1ms;
    pacer.Service();
    EXPECT_EQ(steps, 1u);
}

TEST_F(TestPacer, a_backlog_within_the_limit_is_caught_up)
{
    now += 50ms;
    pacer.Service();

    EXPECT_EQ(steps, 50u);
    EXPECT_EQ(pacer.Overruns(), 0u);
}

TEST_F(TestPacer, the_real_time_factor_is_virtual_over_wall_time)
{
    now += 10ms;
    pacer.Service();
    EXPECT_NEAR(pacer.RealTimeFactor(), 1.0f, 1e-6f);

    now += 100ms;
    pacer.Service();
    EXPECT_NEAR(pacer.RealTimeFactor(), 10.0f / 110.0f, 1e-6f);
}

TEST_F(TestPacer, virtual_time_counts_the_steps_run)
{
    now += 7ms;
    pacer.Service();

    EXPECT_EQ(pacer.VirtualTime(), 7ms);
}
