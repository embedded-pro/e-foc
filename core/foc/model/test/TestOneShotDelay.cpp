#include "core/foc/model/OneShotDelay.hpp"
#include <gtest/gtest.h>

namespace
{
    class OneShotDelayTest
        : public ::testing::Test
    {
    protected:
        foc::OneShotDelay delay;
    };
}

TEST_F(OneShotDelayTest, unconfigured_delay_never_fires)
{
    delay.Begin();
    for (uint32_t tick = 0; tick != 10; ++tick)
        EXPECT_FALSE(delay.Fire(tick));
}

TEST_F(OneShotDelayTest, unscheduled_delay_never_fires)
{
    delay.Configure(std::nullopt);
    delay.Begin();

    EXPECT_FALSE(delay.Fire(0));
    EXPECT_FALSE(delay.Fire(1));
}

TEST_F(OneShotDelayTest, fires_once_after_the_delay)
{
    delay.Configure(3u);
    delay.Begin();

    EXPECT_FALSE(delay.Fire(100));
    EXPECT_FALSE(delay.Fire(101));
    EXPECT_FALSE(delay.Fire(102));
    EXPECT_TRUE(delay.Fire(103));
    EXPECT_FALSE(delay.Fire(104));
}

TEST_F(OneShotDelayTest, zero_delay_fires_on_the_begin_tick)
{
    delay.Configure(0u);
    delay.Begin();

    EXPECT_TRUE(delay.Fire(7));
}

TEST_F(OneShotDelayTest, does_not_fire_without_begin)
{
    delay.Configure(0u);

    EXPECT_FALSE(delay.Fire(0));
    EXPECT_FALSE(delay.Fire(1));
}

TEST_F(OneShotDelayTest, begin_re_arms_for_the_next_run)
{
    delay.Configure(1u);
    delay.Begin();
    delay.Fire(0);
    EXPECT_TRUE(delay.Fire(1));

    delay.Begin();
    EXPECT_FALSE(delay.Fire(50));
    EXPECT_TRUE(delay.Fire(51));
}
