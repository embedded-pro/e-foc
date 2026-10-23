#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include <gtest/gtest.h>

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
