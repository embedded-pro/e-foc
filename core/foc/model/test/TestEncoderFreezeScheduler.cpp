#include "core/foc/model/EncoderFreezeScheduler.hpp"
#include <gtest/gtest.h>

namespace
{
    class EncoderFreezeSchedulerTest
        : public ::testing::Test
    {
    protected:
        foc::EncoderFreezeScheduler scheduler;
    };
}

TEST_F(EncoderFreezeSchedulerTest, unconfigured_scheduler_never_fires)
{
    scheduler.Begin();
    for (uint32_t tick = 0; tick != 10; ++tick)
        EXPECT_FALSE(scheduler.Fire(tick));
}

TEST_F(EncoderFreezeSchedulerTest, fires_once_after_the_delay)
{
    scheduler.Configure(3);
    scheduler.Begin();

    EXPECT_FALSE(scheduler.Fire(100));
    EXPECT_FALSE(scheduler.Fire(101));
    EXPECT_FALSE(scheduler.Fire(102));
    EXPECT_TRUE(scheduler.Fire(103));
    EXPECT_FALSE(scheduler.Fire(104));
}

// Unlike the torque step, whose magnitude says whether it is scheduled, the delay is all this
// scheduler carries, so zero has to mean unscheduled. An encoder that is already stuck before the
// drive aligns is the boot-time fault flag, not a freeze of zero delay.
TEST_F(EncoderFreezeSchedulerTest, zero_delay_never_fires)
{
    scheduler.Configure(0);
    scheduler.Begin();

    EXPECT_FALSE(scheduler.Fire(0));
    EXPECT_FALSE(scheduler.Fire(1));
}

TEST_F(EncoderFreezeSchedulerTest, does_not_fire_without_begin)
{
    scheduler.Configure(1);

    EXPECT_FALSE(scheduler.Fire(0));
    EXPECT_FALSE(scheduler.Fire(1));
}

TEST_F(EncoderFreezeSchedulerTest, begin_re_arms_for_the_next_run)
{
    scheduler.Configure(1);
    scheduler.Begin();
    scheduler.Fire(0);
    EXPECT_TRUE(scheduler.Fire(1));

    scheduler.Begin();
    EXPECT_FALSE(scheduler.Fire(50));
    EXPECT_TRUE(scheduler.Fire(51));
}
