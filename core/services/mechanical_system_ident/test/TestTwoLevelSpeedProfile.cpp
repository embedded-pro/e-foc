#include "core/services/mechanical_system_ident/TwoLevelSpeedProfile.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gtest/gtest.h"

namespace
{
    using services::TwoLevelSpeedProfile;

    constexpr float period{ 0.001f };

    class TestTwoLevelSpeedProfile
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        TwoLevelSpeedProfile profile;
    };
}

TEST_F(TestTwoLevelSpeedProfile, the_command_slews_from_standstill_to_the_dwell_level)
{
    profile.Start(TwoLevelSpeedProfile::Levels{ 2.5f, 5.0f, 1000.0f }, std::chrono::milliseconds{ 100 });

    EXPECT_NEAR(1.0f, profile.Step(period), 1e-6f);
    EXPECT_NEAR(2.0f, profile.Step(period), 1e-6f);
    EXPECT_NEAR(2.5f, profile.Step(period), 1e-6f);
    EXPECT_NEAR(2.5f, profile.Step(period), 1e-6f);
}

TEST_F(TestTwoLevelSpeedProfile, every_dwell_time_the_level_alternates)
{
    profile.Start(TwoLevelSpeedProfile::Levels{ 1.0f, 3.0f, 1000.0f }, std::chrono::milliseconds{ 100 });
    profile.Step(period);

    ForwardTime(std::chrono::milliseconds{ 100 });
    EXPECT_NEAR(2.0f, profile.Step(period), 1e-6f);
    EXPECT_NEAR(3.0f, profile.Step(period), 1e-6f);

    ForwardTime(std::chrono::milliseconds{ 100 });
    EXPECT_NEAR(2.0f, profile.Step(period), 1e-6f);
    EXPECT_NEAR(1.0f, profile.Step(period), 1e-6f);
}

TEST_F(TestTwoLevelSpeedProfile, a_stopped_profile_stays_on_its_level)
{
    profile.Start(TwoLevelSpeedProfile::Levels{ 1.0f, 3.0f, 1000.0f }, std::chrono::milliseconds{ 100 });
    profile.Stop();

    ForwardTime(std::chrono::milliseconds{ 300 });
    EXPECT_NEAR(1.0f, profile.Step(period), 1e-6f);
    EXPECT_NEAR(1.0f, profile.Step(period), 1e-6f);
}
