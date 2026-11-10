#include "tools/virtual_motor/app/logic/TraceHistory.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace virtual_motor;

    port::Window WindowAt(uint32_t sequence, float value)
    {
        port::Window window{ sequence, uint64_t{ sequence } * 10, 10, {}, {} };
        window.minimum.fill(value - 1.0f);
        window.maximum.fill(value + 1.0f);
        return window;
    }
}

TEST(TestTraceHistory, keeps_windows_in_order_and_starts_empty)
{
    app::TraceHistory history{ 4 };
    EXPECT_EQ(history.Count(), 0u);

    history.Add(WindowAt(0, 1.0f));
    history.Add(WindowAt(1, 2.0f));

    ASSERT_EQ(history.Count(), 2u);
    EXPECT_EQ(history.At(0).firstPeriod, 0u);
    EXPECT_EQ(history.At(1).firstPeriod, 10u);
    EXPECT_NEAR(history.At(1).minimum[3], 1.0f, 1e-6f);
    EXPECT_NEAR(history.At(1).maximum[3], 3.0f, 1e-6f);
    EXPECT_FALSE(history.At(1).afterGap);
}

TEST(TestTraceHistory, drops_the_oldest_window_once_full)
{
    app::TraceHistory history{ 3 };
    for (uint32_t sequence = 0; sequence != 5; ++sequence)
        history.Add(WindowAt(sequence, 0.0f));

    ASSERT_EQ(history.Count(), 3u);
    EXPECT_EQ(history.At(0).firstPeriod, 20u);
    EXPECT_EQ(history.At(2).firstPeriod, 40u);
}

TEST(TestTraceHistory, a_gap_in_the_sequence_marks_the_next_window_and_counts_what_was_dropped)
{
    app::TraceHistory history{ 8 };
    history.Add(WindowAt(0, 0.0f));
    history.Add(WindowAt(3, 0.0f));
    history.Add(WindowAt(4, 0.0f));

    EXPECT_TRUE(history.At(1).afterGap);
    EXPECT_FALSE(history.At(2).afterGap);
    EXPECT_EQ(history.Dropped(), 2u);
}

TEST(TestTraceHistory, clearing_forgets_the_windows_and_the_drops_and_moves_the_revision)
{
    app::TraceHistory history{ 8 };
    history.Add(WindowAt(0, 0.0f));
    history.Add(WindowAt(2, 0.0f));
    const auto revision = history.Revision();

    history.Clear();
    EXPECT_EQ(history.Count(), 0u);
    EXPECT_EQ(history.Dropped(), 0u);
    EXPECT_NE(history.Revision(), revision);

    history.Add(WindowAt(7, 0.0f));
    EXPECT_FALSE(history.At(0).afterGap);
    EXPECT_EQ(history.Dropped(), 0u);
}
