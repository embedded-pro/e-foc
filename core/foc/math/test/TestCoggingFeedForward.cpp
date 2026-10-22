#include "core/foc/math/CoggingFeedForward.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float twoPi{ 2.0f * std::numbers::pi_v<float> };
    constexpr std::size_t bins{ 48 };
    constexpr float cyclesPerPeriod{ 6.0f };
    constexpr float amplitude{ 4.0e-3f };
    constexpr float torqueConstant{ 0.042f };

    class TestCoggingFeedForward
        : public ::testing::Test
    {
    public:
        static std::array<int8_t, bins> SampledWave()
        {
            std::array<int8_t, bins> table{};
            for (std::size_t bin = 0; bin != bins; ++bin)
                table[bin] = static_cast<int8_t>(std::lround(127.0f * std::sin(cyclesPerPeriod * (static_cast<float>(bin) + 0.5f) * twoPi / bins)));
            return table;
        }

        void LoadWave(float kt = torqueConstant)
        {
            const auto table = SampledWave();
            feedForward.Load(infra::MakeRange(table), amplitude / 127.0f, foc::NewtonMeter{ kt });
        }

        foc::CoggingFeedForward feedForward;
    };
}

TEST_F(TestCoggingFeedForward, without_a_map_no_current_is_asked_for)
{
    for (float angle = -10.0f; angle < 10.0f; angle += 0.37f)
        EXPECT_EQ(feedForward.CurrentAt(angle), 0.0f);
}

TEST_F(TestCoggingFeedForward, a_map_sampled_eight_times_a_cycle_reproduces_the_wave_between_its_bins)
{
    LoadWave();

    const auto peak = amplitude / torqueConstant;
    float worst{ 0.0f };
    for (float angle = 0.0f; angle < twoPi; angle += 0.001f)
        worst = std::max(worst, std::abs(feedForward.CurrentAt(angle) - peak * std::sin(cyclesPerPeriod * angle)));

    EXPECT_LT(worst, 0.03f * peak);
}

TEST_F(TestCoggingFeedForward, the_map_repeats_every_electrical_period_in_either_direction)
{
    LoadWave();

    for (float angle = 0.0f; angle < twoPi; angle += 0.21f)
    {
        EXPECT_NEAR(feedForward.CurrentAt(angle + 3.0f * twoPi), feedForward.CurrentAt(angle), 1e-4f);
        EXPECT_NEAR(feedForward.CurrentAt(angle - 2.0f * twoPi), feedForward.CurrentAt(angle), 1e-4f);
    }
}

TEST_F(TestCoggingFeedForward, a_stronger_motor_needs_proportionally_less_current)
{
    LoadWave();
    const auto nominal = feedForward.CurrentAt(0.2f);

    LoadWave(2.0f * torqueConstant);

    EXPECT_NEAR(feedForward.CurrentAt(0.2f), 0.5f * nominal, 1e-6f);
}

TEST_F(TestCoggingFeedForward, clearing_the_map_stops_the_feed_forward)
{
    LoadWave();

    feedForward.Clear();

    EXPECT_EQ(feedForward.CurrentAt(0.2f), 0.0f);
}
