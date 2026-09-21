#include "core/foc/model/TorqueStepScheduler.hpp"
#include <gtest/gtest.h>

namespace
{
    class TorqueStepSchedulerTest
        : public ::testing::Test
    {
    protected:
        foc::TorqueStepScheduler scheduler;
    };
}

TEST_F(TorqueStepSchedulerTest, unconfigured_scheduler_never_fires)
{
    scheduler.Begin();
    for (uint32_t tick = 0; tick != 10; ++tick)
        EXPECT_FALSE(scheduler.Fire(tick).has_value());
}

TEST_F(TorqueStepSchedulerTest, fires_once_after_the_delay)
{
    scheduler.Configure({ 0.002f, 3 });
    scheduler.Begin();

    EXPECT_FALSE(scheduler.Fire(100).has_value());
    EXPECT_FALSE(scheduler.Fire(101).has_value());
    EXPECT_FALSE(scheduler.Fire(102).has_value());

    const auto fired = scheduler.Fire(103);
    ASSERT_TRUE(fired.has_value());
    EXPECT_FLOAT_EQ(*fired, 0.002f);

    EXPECT_FALSE(scheduler.Fire(104).has_value());
}

TEST_F(TorqueStepSchedulerTest, zero_delay_fires_on_the_begin_tick)
{
    scheduler.Configure({ -0.001f, 0 });
    scheduler.Begin();

    const auto fired = scheduler.Fire(7);
    ASSERT_TRUE(fired.has_value());
    EXPECT_FLOAT_EQ(*fired, -0.001f);
}

TEST_F(TorqueStepSchedulerTest, does_not_fire_without_begin)
{
    scheduler.Configure({ 0.002f, 0 });

    EXPECT_FALSE(scheduler.Fire(0).has_value());
}

TEST_F(TorqueStepSchedulerTest, begin_re_arms_for_the_next_run)
{
    scheduler.Configure({ 0.002f, 1 });
    scheduler.Begin();
    scheduler.Fire(0);
    EXPECT_TRUE(scheduler.Fire(1).has_value());

    scheduler.Begin();
    EXPECT_FALSE(scheduler.Fire(50).has_value());
    EXPECT_TRUE(scheduler.Fire(51).has_value());
}
