#include "tools/virtual_motor/platform/StreamWindow.hpp"
#include "gtest/gtest.h"

namespace
{
    using virtual_motor::port::signalCount;

    std::array<float, signalCount> Sample(float value)
    {
        std::array<float, signalCount> sample{};
        sample.fill(value);
        return sample;
    }
}

TEST(TestStreamWindow, closes_a_window_every_ten_periods_with_each_signals_extremes)
{
    virtual_motor::StreamWindow window;
    window.Begin();

    for (uint64_t period = 100; period != 109; ++period)
        EXPECT_FALSE(window.Add(period, Sample(static_cast<float>(period % 7))).has_value());
    const auto closed = window.Add(109, Sample(-2.0f));

    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed->sequence, 0u);
    EXPECT_EQ(closed->firstPeriod, 100u);
    EXPECT_EQ(closed->periods, 10);
    EXPECT_FLOAT_EQ(closed->minimum[0], -2.0f);
    EXPECT_FLOAT_EQ(closed->maximum[signalCount - 1], 6.0f);
    EXPECT_FLOAT_EQ(closed->last[3], -2.0f);
}

TEST(TestStreamWindow, each_closed_window_takes_the_next_sequence_number)
{
    virtual_motor::StreamWindow window;
    window.Begin();

    std::optional<virtual_motor::port::Window> last;
    for (uint64_t period = 0; period != 30; ++period)
        if (auto closed = window.Add(period, Sample(1.0f)))
            last = closed;

    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->sequence, 2u);
    EXPECT_EQ(last->firstPeriod, 20u);
}

TEST(TestStreamWindow, beginning_again_restarts_the_sequence_and_drops_a_partial_window)
{
    virtual_motor::StreamWindow window;
    window.Begin();
    for (uint64_t period = 0; period != 15; ++period)
        window.Add(period, Sample(1.0f));

    window.Begin();
    std::optional<virtual_motor::port::Window> closed;
    for (uint64_t period = 50; period != 60; ++period)
        closed = window.Add(period, Sample(3.0f));

    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed->sequence, 0u);
    EXPECT_EQ(closed->firstPeriod, 50u);
    EXPECT_FLOAT_EQ(closed->minimum[3], 3.0f);
}
