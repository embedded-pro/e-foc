#include "core/foc/math/MechanicalWindowAccumulator.hpp"
#include <gmock/gmock.h>

namespace
{
    class TestMechanicalWindowAccumulator
        : public ::testing::Test
    {
    public:
        static constexpr uint32_t samples{ 20 };
        static constexpr float tolerance{ 1e-6f };
        foc::MechanicalWindowAccumulator accumulator{ samples };

        foc::MechanicalWindow Fill(float iq, float step)
        {
            for (uint32_t sample = 0; sample != samples; ++sample)
                accumulator.Accumulate(iq, step);

            return accumulator.Close();
        }
    };
}

TEST_F(TestMechanicalWindowAccumulator, a_constant_current_gives_the_moments_of_u_over_the_window)
{
    const auto window = Fill(2.0f, 0.0f);
    const auto n = static_cast<float>(samples);

    EXPECT_NEAR(window.meanIq.Value(), 2.0f, tolerance);
    EXPECT_NEAR(window.meanIqTimesU, 2.0f * 0.5f, tolerance);
    EXPECT_NEAR(window.meanIqTimesUSquared, 2.0f * (1.0f / 3.0f - 1.0f / (12.0f * n * n)), tolerance);
}

TEST_F(TestMechanicalWindowAccumulator, a_constant_step_gives_the_rotation_and_the_mean_position_from_the_start)
{
    const auto window = Fill(0.0f, 0.01f);
    const auto n = static_cast<float>(samples);

    EXPECT_NEAR(window.rotation.Value(), 0.01f * n, tolerance);
    EXPECT_NEAR(window.meanPosition.Value(), 0.01f * (n + 1.0f) / 2.0f, tolerance);
}

TEST_F(TestMechanicalWindowAccumulator, the_smallest_step_ignores_samples_without_motion)
{
    accumulator.Accumulate(0.0f, 0.0f);
    accumulator.Accumulate(0.0f, -0.003f);
    accumulator.Accumulate(0.0f, 0.002f);
    accumulator.Accumulate(0.0f, 0.0f);

    EXPECT_NEAR(accumulator.Close().smallestStep.Value(), 0.002f, tolerance);
    EXPECT_EQ(Fill(0.0f, 0.0f).smallestStep.Value(), 0.0f);
}

TEST_F(TestMechanicalWindowAccumulator, only_the_first_window_after_a_restart_is_marked_first)
{
    EXPECT_TRUE(Fill(0.0f, 0.0f).first);
    EXPECT_FALSE(Fill(0.0f, 0.0f).first);

    accumulator.Restart();

    EXPECT_TRUE(Fill(0.0f, 0.0f).first);
}

TEST_F(TestMechanicalWindowAccumulator, closing_starts_the_next_window_from_zero)
{
    Fill(1.0f, 0.01f);
    const auto window = Fill(0.0f, 0.0f);

    EXPECT_EQ(window.rotation.Value(), 0.0f);
    EXPECT_EQ(window.meanPosition.Value(), 0.0f);
    EXPECT_EQ(window.meanIq.Value(), 0.0f);
}
