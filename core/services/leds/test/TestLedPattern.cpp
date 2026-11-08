#include "core/services/leds/LedPattern.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <string>

namespace
{
    using state_machine::FaultCode;

    std::string Render(const services::LedPattern& pattern, uint32_t ticks)
    {
        std::string levels;
        for (uint32_t tick = 0; tick != ticks; ++tick)
            levels += pattern.LevelAt(tick) ? '#' : '.';
        return levels;
    }

    uint32_t FlashesOf(FaultCode code)
    {
        const auto pattern = services::PatternsFor({ services::LedLifecycle::fault, code }).red;
        uint32_t flashes{ 0 };
        bool previous{ false };
        for (uint32_t tick = 0; tick != pattern.PeriodTicks(); ++tick)
        {
            const bool level = pattern.LevelAt(tick);
            flashes += (level && !previous) ? 1 : 0;
            previous = level;
        }
        return flashes;
    }
}

TEST(LedPatternTest, off_is_never_lit)
{
    EXPECT_EQ(std::string(8, '.'), Render(services::LedPattern::Off(), 8));
}

TEST(LedPatternTest, steady_is_always_lit)
{
    EXPECT_EQ(std::string(8, '#'), Render(services::LedPattern::Steady(), 8));
}

TEST(LedPatternTest, a_flash_group_is_followed_by_its_pause_and_repeats)
{
    const auto pattern = services::LedPattern::Flashes(2, 2, 1, 3);

    EXPECT_EQ(9u, pattern.PeriodTicks());
    EXPECT_EQ("##.##....##.##....", Render(pattern, 18));
}

TEST(LedPatternTest, the_heartbeat_is_50_ms_on_every_2_s)
{
    const auto pattern = services::PatternsFor({ services::LedLifecycle::ready, FaultCode::none }).green;

    EXPECT_EQ(40u, pattern.PeriodTicks());
    EXPECT_TRUE(pattern.LevelAt(0));
    EXPECT_FALSE(pattern.LevelAt(1));
    EXPECT_FALSE(pattern.LevelAt(39));
    EXPECT_TRUE(pattern.LevelAt(40));
}

TEST(LedPatternTest, the_slow_blink_is_half_a_second_on_and_off)
{
    const auto amber = services::PatternsFor({ services::LedLifecycle::idle, FaultCode::none }).amber;

    EXPECT_EQ(20u, amber.PeriodTicks());
    EXPECT_TRUE(amber.LevelAt(9));
    EXPECT_FALSE(amber.LevelAt(10));
    EXPECT_FALSE(amber.LevelAt(19));
    EXPECT_TRUE(amber.LevelAt(20));
}

TEST(LedPatternTest, a_fault_code_flashes_250_ms_on_and_off_then_pauses_1_5_s)
{
    const auto red = services::PatternsFor({ services::LedLifecycle::fault, FaultCode::overcurrent }).red;

    EXPECT_EQ(std::string("#####.....#####.....") + std::string(30, '.'), Render(red, 50));
    EXPECT_EQ(50u, red.PeriodTicks());
}

TEST(LedPatternTest, no_fault_leaves_red_off_in_every_state_but_fault)
{
    for (auto lifecycle : { services::LedLifecycle::idle, services::LedLifecycle::calibrating, services::LedLifecycle::ready, services::LedLifecycle::enabled })
        EXPECT_EQ(services::LedPattern::Off(), services::PatternsFor({ lifecycle, FaultCode::none }).red);
}

TEST(LedPatternTest, every_fault_class_flashes_its_own_count)
{
    EXPECT_EQ(2u, FlashesOf(FaultCode::overcurrent));
    EXPECT_EQ(3u, FlashesOf(FaultCode::overvoltage));
    EXPECT_EQ(4u, FlashesOf(FaultCode::overtemperature));
    EXPECT_EQ(5u, FlashesOf(FaultCode::encoderLoss));
    EXPECT_EQ(6u, FlashesOf(FaultCode::undervoltage));
    EXPECT_EQ(8u, FlashesOf(FaultCode::commandTimeout));
}

TEST(LedPatternTest, the_motion_supervision_faults_share_seven_flashes)
{
    for (auto code : { FaultCode::stall, FaultCode::phaseMissing, FaultCode::overspeed, FaultCode::followingError, FaultCode::positionLimit })
        EXPECT_EQ(7u, FlashesOf(code));
}

TEST(LedPatternTest, a_fault_without_a_code_of_its_own_is_steady_red)
{
    for (auto code : { FaultCode::hardwareFault, FaultCode::calibrationFailed, FaultCode::none, static_cast<FaultCode>(200) })
        EXPECT_EQ(services::LedPattern::Steady(), services::PatternsFor({ services::LedLifecycle::fault, code }).red);
}

TEST(LedPatternTest, a_latched_fault_never_leaves_red_dark_longer_than_the_pause)
{
    for (auto code : { FaultCode::overcurrent, FaultCode::commandTimeout, FaultCode::stall })
    {
        const auto red = services::PatternsFor({ services::LedLifecycle::fault, code }).red;
        uint32_t dark{ 0 };
        uint32_t longestDark{ 0 };
        for (uint32_t tick = 0; tick != 2 * red.PeriodTicks(); ++tick)
        {
            dark = red.LevelAt(tick) ? 0 : dark + 1;
            longestDark = std::max(longestDark, dark);
        }
        EXPECT_LE(longestDark, 30u + 5u);
    }
}

TEST(LedPatternTest, green_follows_the_state)
{
    const auto green = [](services::LedLifecycle lifecycle)
    {
        return services::PatternsFor({ lifecycle, FaultCode::none }).green;
    };

    EXPECT_EQ(40u, green(services::LedLifecycle::idle).PeriodTicks());
    EXPECT_EQ(40u, green(services::LedLifecycle::calibrating).PeriodTicks());
    EXPECT_EQ(40u, green(services::LedLifecycle::ready).PeriodTicks());
    EXPECT_EQ(services::LedPattern::Steady(), green(services::LedLifecycle::enabled));
    EXPECT_EQ(services::LedPattern::Off(), services::PatternsFor({ services::LedLifecycle::fault, FaultCode::overcurrent }).green);
}

TEST(LedPatternTest, amber_follows_the_state)
{
    const auto amber = [](services::LedLifecycle lifecycle)
    {
        return services::PatternsFor({ lifecycle, FaultCode::none }).amber;
    };

    EXPECT_EQ(20u, amber(services::LedLifecycle::idle).PeriodTicks());
    EXPECT_EQ(services::LedPattern::Steady(), amber(services::LedLifecycle::calibrating));
    EXPECT_EQ(services::LedPattern::Off(), amber(services::LedLifecycle::ready));
    EXPECT_EQ(services::LedPattern::Off(), amber(services::LedLifecycle::enabled));
    EXPECT_EQ(services::LedPattern::Off(), amber(services::LedLifecycle::fault));
}
