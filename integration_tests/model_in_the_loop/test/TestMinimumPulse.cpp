#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>

namespace
{
    constexpr float minimumPulseNanoseconds{ 500.0f };
    constexpr float highSpeed{ 400.0f };
    constexpr auto settle = std::chrono::milliseconds{ 400 };
    constexpr auto observe = std::chrono::milliseconds{ 200 };

    struct PulseCount
    {
        uint32_t ticks{ 0 };
        uint32_t shifted{ 0 };
        uint32_t tooShort{ 0 };
    };

    mil::Scenario BenchAtHighModulation(float commandedMinimumPulseNanoseconds)
    {
        mil::Scenario scenario;
        scenario.Plant("bench").Calibrated().BootsIn(state_machine::ControlMode::speed);
        sil::DescribeBenchCurrentSense(scenario.plant);
        sil::DescribeBenchPowerStage(scenario.plant);
        scenario.plant.minimumPulseNanoseconds = commandedMinimumPulseNanoseconds;
        return scenario;
    }

    PulseCount CountPulses(mil::ModelInTheLoop& mil, uint32_t shortLimit)
    {
        PulseCount count;
        mil.RunUntil([&mil, &count, shortLimit]()
            {
                const auto& duty = mil.Drive().CommandedDuty();
                const auto legs = { duty.a.Value(), duty.b.Value(), duty.c.Value() };
                ++count.ticks;
                if (*std::min_element(legs.begin(), legs.end()) == 0)
                    ++count.shifted;
                if (std::any_of(legs.begin(), legs.end(), [shortLimit](uint32_t leg)
                        {
                            return leg != 0 && leg < shortLimit;
                        }))
                    ++count.tooShort;
                return false;
            },
            observe);
        return count;
    }

    PulseCount RunAtHighSpeed(float commandedMinimumPulseNanoseconds)
    {
        mil::ModelInTheLoop mil{ BenchAtHighModulation(commandedMinimumPulseNanoseconds) };
        EXPECT_TRUE(mil.Align());
        EXPECT_TRUE(mil.Enable());
        EXPECT_TRUE(mil.SetSpeed(highSpeed));
        mil.Run(settle);
        EXPECT_TRUE(mil.IsRunning());

        const auto shortLimit = foc::MinimumPulseDuty(minimumPulseNanoseconds, 20000u).Value();
        const auto count = CountPulses(mil, shortLimit);
        std::fprintf(stderr, "[METRIC] mil minimum pulse min_ns=%.0f ticks=%u shifted=%u too_short=%u\n",
            commandedMinimumPulseNanoseconds, count.ticks, count.shifted, count.tooShort);
        return count;
    }

    class TestMinimumPulse
        : public ::testing::Test
    {};
}

TEST_F(TestMinimumPulse, without_the_handling_the_shift_leaves_pulses_shorter_than_the_dead_band_at_high_modulation)
{
    const auto count = RunAtHighSpeed(0.0f);

    EXPECT_GT(count.shifted, 0u);
    EXPECT_GT(count.tooShort, 0u);
}

TEST_F(TestMinimumPulse, with_the_handling_no_leg_is_left_shorter_than_the_dead_band_at_high_modulation)
{
    const auto count = RunAtHighSpeed(minimumPulseNanoseconds);

    EXPECT_GT(count.shifted, 0u);
    EXPECT_EQ(count.tooShort, 0u);
}
