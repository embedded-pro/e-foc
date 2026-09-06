#include "core/platform_abstraction/ControlLoopMetrics.hpp"
#include <gtest/gtest.h>

namespace
{
    constexpr uint32_t budget = 4500;
    constexpr uint32_t period = 6000;

    class TestControlLoopMetrics
        : public ::testing::Test
    {
    public:
        TestControlLoopMetrics()
        {
            metrics.Configure(budget, period);
        }

        application::ControlLoopMetrics metrics;
    };
}

TEST_F(TestControlLoopMetrics, an_unconfigured_channel_reports_nothing)
{
    application::ControlLoopMetrics fresh;

    const auto statistics = fresh.Read();

    EXPECT_EQ(0u, statistics.samples);
    EXPECT_EQ(0u, statistics.minimumCycles);
    EXPECT_EQ(0u, statistics.maximumCycles);
    EXPECT_EQ(0u, statistics.overruns);
}

TEST_F(TestControlLoopMetrics, the_first_sample_sets_every_extreme)
{
    metrics.Record(1200);

    const auto statistics = metrics.Read();

    EXPECT_EQ(1u, statistics.samples);
    EXPECT_EQ(1200u, statistics.lastCycles);
    EXPECT_EQ(1200u, statistics.minimumCycles);
    EXPECT_EQ(1200u, statistics.maximumCycles);
    EXPECT_EQ(1200u, statistics.averageCycles);
}

TEST_F(TestControlLoopMetrics, the_extremes_track_the_run)
{
    metrics.Record(2000);
    metrics.Record(1000);
    metrics.Record(3000);

    const auto statistics = metrics.Read();

    EXPECT_EQ(3u, statistics.samples);
    EXPECT_EQ(3000u, statistics.lastCycles);
    EXPECT_EQ(1000u, statistics.minimumCycles);
    EXPECT_EQ(3000u, statistics.maximumCycles);
}

TEST_F(TestControlLoopMetrics, the_average_settles_on_a_steady_duration)
{
    for (int sample = 0; sample != 200; ++sample)
        metrics.Record(1800);

    EXPECT_EQ(1800u, metrics.Read().averageCycles);
}

TEST_F(TestControlLoopMetrics, the_average_moves_toward_a_new_duration_without_jumping_to_it)
{
    for (int sample = 0; sample != 200; ++sample)
        metrics.Record(1000);

    metrics.Record(5000);

    const auto average = metrics.Read().averageCycles;

    EXPECT_GT(average, 1000u);
    EXPECT_LT(average, 5000u);
}

TEST_F(TestControlLoopMetrics, exceeding_the_budget_counts_an_overrun_but_not_a_deadline_miss)
{
    metrics.Record(budget + 1);

    const auto statistics = metrics.Read();

    EXPECT_EQ(1u, statistics.overruns);
    EXPECT_EQ(0u, statistics.deadlineMisses);
}

TEST_F(TestControlLoopMetrics, exceeding_the_period_counts_both)
{
    metrics.Record(period + 1);

    const auto statistics = metrics.Read();

    EXPECT_EQ(1u, statistics.overruns);
    EXPECT_EQ(1u, statistics.deadlineMisses);
}

TEST_F(TestControlLoopMetrics, a_run_inside_the_budget_counts_neither)
{
    metrics.Record(budget);

    const auto statistics = metrics.Read();

    EXPECT_EQ(0u, statistics.overruns);
    EXPECT_EQ(0u, statistics.deadlineMisses);
}

TEST_F(TestControlLoopMetrics, a_reentry_is_counted_separately_from_a_deadline_miss)
{
    metrics.RecordReentry();
    metrics.RecordReentry();

    const auto statistics = metrics.Read();

    EXPECT_EQ(2u, statistics.reentries);
    EXPECT_EQ(0u, statistics.deadlineMisses);
    EXPECT_EQ(0u, statistics.samples);
}

TEST_F(TestControlLoopMetrics, reset_clears_the_run_but_keeps_the_budget)
{
    metrics.Record(period + 1);
    metrics.RecordReentry();
    metrics.Reset();

    const auto statistics = metrics.Read();

    EXPECT_EQ(0u, statistics.samples);
    EXPECT_EQ(0u, statistics.minimumCycles);
    EXPECT_EQ(0u, statistics.maximumCycles);
    EXPECT_EQ(0u, statistics.overruns);
    EXPECT_EQ(0u, statistics.deadlineMisses);
    EXPECT_EQ(0u, statistics.reentries);
    EXPECT_EQ(budget, statistics.budgetCycles);
}

TEST_F(TestControlLoopMetrics, counters_keep_counting_well_past_a_16_bit_range)
{
    application::ControlLoopMetrics counting;
    counting.Configure(1, 1);

    for (uint32_t sample = 0; sample != 70000; ++sample)
        counting.Record(2);

    const auto statistics = counting.Read();

    EXPECT_EQ(70000u, statistics.samples);
    EXPECT_EQ(70000u, statistics.overruns);
}
