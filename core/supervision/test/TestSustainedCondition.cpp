#include "core/supervision/SustainedCondition.hpp"
#include <chrono>
#include <gtest/gtest.h>

namespace
{
    using namespace std::chrono_literals;

    class SustainedConditionTest
        : public testing::Test
    {
    public:
        infra::TimePoint At(std::chrono::milliseconds offset) const
        {
            return infra::TimePoint{} + offset;
        }

        supervision::SustainedCondition condition;
    };
}

TEST_F(SustainedConditionTest, held_only_once_the_required_duration_has_passed)
{
    EXPECT_FALSE(condition.Held(true, At(0ms), 10ms));
    EXPECT_FALSE(condition.Held(true, At(9ms), 10ms));
    EXPECT_TRUE(condition.Held(true, At(10ms), 10ms));
    EXPECT_TRUE(condition.Held(true, At(20ms), 10ms));
}

TEST_F(SustainedConditionTest, a_zero_duration_is_held_at_once)
{
    EXPECT_TRUE(condition.Held(true, At(0ms), 0ms));
}

TEST_F(SustainedConditionTest, a_false_condition_restarts_the_span)
{
    EXPECT_FALSE(condition.Held(true, At(0ms), 10ms));
    EXPECT_FALSE(condition.Held(false, At(8ms), 10ms));
    EXPECT_FALSE(condition.Held(true, At(9ms), 10ms));
    EXPECT_FALSE(condition.Held(true, At(18ms), 10ms));
    EXPECT_TRUE(condition.Held(true, At(19ms), 10ms));
}

TEST_F(SustainedConditionTest, held_count_needs_that_many_consecutive_evaluations)
{
    EXPECT_FALSE(condition.HeldCount(true, 3));
    EXPECT_FALSE(condition.HeldCount(true, 3));
    EXPECT_TRUE(condition.HeldCount(true, 3));
    EXPECT_TRUE(condition.HeldCount(true, 3));
}

TEST_F(SustainedConditionTest, a_false_evaluation_restarts_the_count)
{
    EXPECT_FALSE(condition.HeldCount(true, 3));
    EXPECT_FALSE(condition.HeldCount(true, 3));
    EXPECT_FALSE(condition.HeldCount(false, 3));
    EXPECT_FALSE(condition.HeldCount(true, 3));
    EXPECT_FALSE(condition.HeldCount(true, 3));
    EXPECT_TRUE(condition.HeldCount(true, 3));
}

TEST_F(SustainedConditionTest, a_trigger_is_reported_once)
{
    EXPECT_FALSE(condition.ReportOnce(false));
    EXPECT_FALSE(condition.Reported());
    EXPECT_TRUE(condition.ReportOnce(true));
    EXPECT_TRUE(condition.Reported());
    EXPECT_FALSE(condition.ReportOnce(true));
}

TEST_F(SustainedConditionTest, reset_clears_span_count_and_report)
{
    EXPECT_TRUE(condition.Held(true, At(0ms), 0ms));
    EXPECT_TRUE(condition.HeldCount(true, 1));
    EXPECT_TRUE(condition.ReportOnce(true));

    condition.Reset();

    EXPECT_FALSE(condition.Reported());
    EXPECT_FALSE(condition.Held(true, At(100ms), 10ms));
    EXPECT_FALSE(condition.HeldCount(true, 2));
    EXPECT_TRUE(condition.ReportOnce(true));
}
