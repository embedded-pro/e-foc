#include "tools/can_commander/logic/FaultHistoryText.hpp"
#include "gtest/gtest.h"
#include <limits>

TEST(TestFaultHistoryText, an_uptime_reads_as_hours_minutes_seconds_and_milliseconds)
{
    EXPECT_EQ("0:00:00.000", tool::UptimeText(0));
    EXPECT_EQ("0:00:01.500", tool::UptimeText(1500));
    EXPECT_EQ("1:02:03.004", tool::UptimeText(3723004));
}

TEST(TestFaultHistoryText, an_uptime_of_days_keeps_counting_hours)
{
    EXPECT_EQ("1193:02:47.295", tool::UptimeText(std::numeric_limits<uint32_t>::max()));
}
