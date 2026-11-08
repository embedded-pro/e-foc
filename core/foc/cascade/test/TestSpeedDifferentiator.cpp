#include "core/foc/cascade/SpeedDifferentiator.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    class TestSpeedDifferentiator
        : public ::testing::Test
    {
    public:
        TestSpeedDifferentiator()
        {
            CloseWindowAt(0.0f);
            differentiator.Measure();
        }

        void CloseWindowAt(float angle)
        {
            differentiator.Track(angle);
            differentiator.CloseWindow();
        }

        static float Coefficient(float timeConstant)
        {
            return 1.0f - std::exp(-0.001f / timeConstant);
        }

        float RippleRms(float speed, bool filtered)
        {
            constexpr float count{ 2.0f * std::numbers::pi_v<float> / 4000.0f };
            constexpr int windows{ 4000 };
            constexpr int settling{ 200 };
            double sumOfSquares{ 0.0 };

            for (int window = 1; window <= windows; ++window)
            {
                const auto ideal = static_cast<double>(speed) * 0.001 * window;
                CloseWindowAt(static_cast<float>(std::floor(ideal / count) * count));
                const auto raw = differentiator.Measure();
                const auto error = static_cast<double>(filtered ? differentiator.FilteredSpeed() : raw) - speed;
                if (window > settling)
                    sumOfSquares += error * error;
            }

            return static_cast<float>(std::sqrt(sumOfSquares / (windows - settling)));
        }

        foc::SpeedDifferentiator differentiator{ hal::Hertz{ 1000 } };
    };
}

TEST_F(TestSpeedDifferentiator, one_window_gives_the_angle_change_over_the_period)
{
    CloseWindowAt(0.01f);

    EXPECT_NEAR(differentiator.Measure(), 10.0f, 1e-3f);
}

TEST_F(TestSpeedDifferentiator, a_delayed_reader_divides_by_every_window_it_missed)
{
    CloseWindowAt(0.01f);
    CloseWindowAt(0.02f);

    EXPECT_NEAR(differentiator.Measure(), 10.0f, 1e-3f);
}

TEST_F(TestSpeedDifferentiator, a_reader_with_no_new_window_keeps_the_last_speed)
{
    CloseWindowAt(0.01f);
    differentiator.Measure();

    EXPECT_NEAR(differentiator.Measure(), 10.0f, 1e-3f);
}

TEST_F(TestSpeedDifferentiator, restart_takes_a_new_reference_window)
{
    CloseWindowAt(0.01f);
    differentiator.Measure();

    differentiator.Restart();
    CloseWindowAt(1.0f);

    EXPECT_NEAR(differentiator.Measure(), 0.0f, 1e-6f);
    CloseWindowAt(1.005f);
    EXPECT_NEAR(differentiator.Measure(), 5.0f, 1e-3f);
}

TEST_F(TestSpeedDifferentiator, without_a_time_constant_the_filtered_speed_is_the_raw_speed)
{
    CloseWindowAt(0.01f);
    differentiator.Measure();

    EXPECT_NEAR(differentiator.FilteredSpeed(), 10.0f, 1e-3f);
}

TEST_F(TestSpeedDifferentiator, the_filtered_speed_steps_up_by_the_coefficient_of_the_time_constant)
{
    differentiator.SetFilterTimeConstant(0.00125f);
    CloseWindowAt(0.01f);
    const auto raw = differentiator.Measure();

    EXPECT_NEAR(raw, 10.0f, 1e-3f);
    EXPECT_NEAR(differentiator.FilteredSpeed(), 10.0f * Coefficient(0.00125f), 1e-3f);
}

TEST_F(TestSpeedDifferentiator, the_filtered_speed_converges_on_a_constant_speed)
{
    differentiator.SetFilterTimeConstant(0.00125f);

    for (int window = 1; window <= 20; ++window)
    {
        CloseWindowAt(0.01f * static_cast<float>(window));
        differentiator.Measure();
    }

    EXPECT_NEAR(differentiator.FilteredSpeed(), 10.0f, 1e-2f);
}

TEST_F(TestSpeedDifferentiator, a_reader_with_no_new_window_does_not_filter_the_same_window_twice)
{
    differentiator.SetFilterTimeConstant(0.00125f);
    CloseWindowAt(0.01f);
    differentiator.Measure();
    const auto once = differentiator.FilteredSpeed();

    differentiator.Measure();

    EXPECT_FLOAT_EQ(differentiator.FilteredSpeed(), once);
}

TEST_F(TestSpeedDifferentiator, alternating_counts_ripple_the_filtered_speed_by_the_expected_fraction_of_the_raw_ripple)
{
    constexpr float count{ 2.0f * std::numbers::pi_v<float> / 4000.0f };
    differentiator.SetFilterTimeConstant(0.00125f);
    double rawSumOfSquares{ 0.0 };
    double filteredSumOfSquares{ 0.0 };
    float angle{ 0.0f };

    for (int window = 1; window <= 400; ++window)
    {
        angle += window % 2 == 0 ? count : -count;
        CloseWindowAt(angle);
        const auto raw = differentiator.Measure();
        if (window > 100)
        {
            rawSumOfSquares += static_cast<double>(raw) * raw;
            filteredSumOfSquares += static_cast<double>(differentiator.FilteredSpeed()) * differentiator.FilteredSpeed();
        }
    }

    EXPECT_NEAR(std::sqrt(filteredSumOfSquares / rawSumOfSquares), 0.38, 0.02);
}

TEST_F(TestSpeedDifferentiator, quantisation_noise_on_a_steady_speed_falls_to_the_predicted_ratio)
{
    differentiator.SetFilterTimeConstant(0.00125f);
    const auto rawRipple = RippleRms(37.3f, false);
    differentiator.Restart();
    CloseWindowAt(0.0f);
    differentiator.Measure();
    const auto filteredRipple = RippleRms(37.3f, true);

    EXPECT_NEAR(rawRipple, 0.64f, 0.08f);
    EXPECT_NEAR(filteredRipple / rawRipple, 0.457f, 0.08f);
}

TEST_F(TestSpeedDifferentiator, restart_clears_the_filtered_speed)
{
    differentiator.SetFilterTimeConstant(0.00125f);
    CloseWindowAt(0.01f);
    differentiator.Measure();

    differentiator.Restart();

    EXPECT_FLOAT_EQ(differentiator.FilteredSpeed(), 0.0f);
    CloseWindowAt(1.0f);
    differentiator.Measure();
    CloseWindowAt(1.01f);
    differentiator.Measure();
    EXPECT_NEAR(differentiator.FilteredSpeed(), 10.0f * Coefficient(0.00125f), 1e-3f);
}
