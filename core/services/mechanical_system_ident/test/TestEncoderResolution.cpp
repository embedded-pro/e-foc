#include "core/services/mechanical_system_ident/EncoderResolution.hpp"
#include "gtest/gtest.h"
#include <numbers>

namespace
{
    using services::EncoderResolution;

    constexpr float samplingPeriod{ 1.0f / 20000.0f };

    class TestEncoderResolution
        : public testing::Test
    {
    public:
        void Move(float step, std::size_t samples)
        {
            for (std::size_t sample = 0; sample != samples; ++sample)
            {
                position += step;
                resolution.Sample(position);
            }
        }

        EncoderResolution resolution;
        float position{ 0.0f };
    };
}

TEST_F(TestEncoderResolution, the_resolution_is_unknown_until_eight_steps_moved)
{
    resolution.Restart(position);
    Move(0.0f, 20);
    Move(0.001f, 7);
    EXPECT_FALSE(resolution.Known());

    Move(0.001f, 1);
    EXPECT_TRUE(resolution.Known());
}

TEST_F(TestEncoderResolution, an_encoder_that_never_moved_asks_for_a_single_sample_window)
{
    resolution.Restart(position);
    Move(0.0f, 20);

    EXPECT_EQ(1u, resolution.WindowSamples(samplingPeriod));
}

TEST_F(TestEncoderResolution, a_4000_count_encoder_asks_for_about_four_milliseconds)
{
    const auto count = 2.0f * std::numbers::pi_v<float> / 4000.0f;
    resolution.Restart(position);
    Move(count, 50);

    EXPECT_EQ(80u, resolution.WindowSamples(samplingPeriod));
}

TEST_F(TestEncoderResolution, a_coarse_encoder_is_capped_at_the_longest_window)
{
    resolution.Restart(position);
    Move(0.1f, 50);

    EXPECT_EQ(EncoderResolution::maximumWindowSamples, resolution.WindowSamples(samplingPeriod));
}

TEST_F(TestEncoderResolution, a_jittering_step_widens_the_window_beyond_the_smallest_step)
{
    const auto count = 2.0f * std::numbers::pi_v<float> / 4000.0f;
    resolution.Restart(position);
    Move(count, 50);
    const auto steady = resolution.WindowSamples(samplingPeriod);

    resolution.Restart(position);
    for (int pair = 0; pair != 25; ++pair)
    {
        Move(count, 1);
        Move(3.0f * count, 1);
    }

    EXPECT_GT(resolution.WindowSamples(samplingPeriod), steady);
}

TEST_F(TestEncoderResolution, restarting_forgets_the_steps_seen)
{
    resolution.Restart(position);
    Move(0.001f, 10);
    resolution.Restart(position);

    EXPECT_FALSE(resolution.Known());
}
