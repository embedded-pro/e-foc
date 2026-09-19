#include "core/platform_abstraction/SoftwareWatchdog.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    class SoftwareWatchdogTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr std::chrono::microseconds deadline{ std::chrono::milliseconds(100) };

        void EnableWatchdog()
        {
            watchdog.Enable(deadline, [this]()
                {
                    ++deadlineMissedCount;
                });
        }

        application::SoftwareWatchdog watchdog;
        uint32_t deadlineMissedCount{ 0 };
    };
}

TEST_F(SoftwareWatchdogTest, is_not_enabled_before_enable)
{
    EXPECT_FALSE(watchdog.IsEnabled());
    EXPECT_EQ(std::chrono::microseconds{ 0 }, watchdog.Deadline());
}

TEST_F(SoftwareWatchdogTest, feeding_before_enable_is_allowed)
{
    watchdog.Feed();

    EXPECT_FALSE(watchdog.IsEnabled());
}

TEST_F(SoftwareWatchdogTest, enable_reports_the_configured_deadline)
{
    EnableWatchdog();

    EXPECT_TRUE(watchdog.IsEnabled());
    EXPECT_EQ(deadline, watchdog.Deadline());
    EXPECT_FALSE(watchdog.HasExpired());
}

TEST_F(SoftwareWatchdogTest, a_fed_watchdog_does_not_expire)
{
    EnableWatchdog();

    for (uint32_t iteration = 0; iteration != 20; ++iteration)
    {
        watchdog.Feed();
        ForwardTime(std::chrono::milliseconds(50));
    }

    EXPECT_EQ(0u, deadlineMissedCount);
    EXPECT_FALSE(watchdog.HasExpired());
}

TEST_F(SoftwareWatchdogTest, a_missed_feed_is_reported_within_two_deadlines)
{
    EnableWatchdog();

    ForwardTime(std::chrono::milliseconds(199));
    EXPECT_EQ(0u, deadlineMissedCount);

    ForwardTime(std::chrono::milliseconds(2));
    EXPECT_EQ(1u, deadlineMissedCount);
    EXPECT_TRUE(watchdog.HasExpired());
}

TEST_F(SoftwareWatchdogTest, expiry_after_the_last_feed_is_bounded_by_two_deadlines)
{
    EnableWatchdog();

    ForwardTime(std::chrono::milliseconds(150));
    watchdog.Feed();

    ForwardTime(std::chrono::milliseconds(99));
    EXPECT_EQ(0u, deadlineMissedCount);

    ForwardTime(std::chrono::milliseconds(52));
    EXPECT_EQ(1u, deadlineMissedCount);
}

TEST_F(SoftwareWatchdogTest, expiry_is_reported_once)
{
    EnableWatchdog();

    ForwardTime(std::chrono::milliseconds(1000));

    EXPECT_EQ(1u, deadlineMissedCount);
}

TEST_F(SoftwareWatchdogTest, feeding_after_expiry_does_not_restart_supervision)
{
    EnableWatchdog();

    ForwardTime(std::chrono::milliseconds(1000));
    ASSERT_EQ(1u, deadlineMissedCount);

    watchdog.Feed();
    ForwardTime(std::chrono::milliseconds(1000));

    EXPECT_EQ(1u, deadlineMissedCount);
    EXPECT_TRUE(watchdog.HasExpired());
}
