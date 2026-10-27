#include "core/foc/cascade/EstimatorWindowAccumulator.hpp"
#include <gtest/gtest.h>

namespace
{
    class EstimatorWindowAccumulatorTest
        : public ::testing::Test
    {
    public:
        static float MeanDirectVoltageOverRamp(uint32_t dutyUpdateDelayPeriods)
        {
            constexpr uint32_t ticks = 4;
            foc::EstimatorWindowAccumulator accumulator{ hal::Hertz{ 20000 }, ticks, dutyUpdateDelayPeriods };

            for (uint32_t tick = 0; tick != ticks; ++tick)
                accumulator.Accumulate(foc::RotatingFrame{ 0.0f, 0.0f }, foc::RotatingFrame{ static_cast<float>(tick + 1), 0.0f }, 0.0f, 4.0f, 0.0f);

            return accumulator.Close().meanNormalizedVd;
        }
    };
}

TEST_F(EstimatorWindowAccumulatorTest, a_duty_that_acts_at_once_is_paired_with_the_current_step_that_follows_it)
{
    EXPECT_NEAR(MeanDirectVoltageOverRamp(0), (0.0f + 1.0f + 2.0f + 3.0f) / 4.0f, 1e-6f);
}

TEST_F(EstimatorWindowAccumulatorTest, a_duty_that_acts_a_period_late_is_paired_with_the_current_step_a_period_later)
{
    EXPECT_NEAR(MeanDirectVoltageOverRamp(1), (0.0f + 0.0f + 1.0f + 2.0f) / 4.0f, 1e-6f);
}

TEST_F(EstimatorWindowAccumulatorTest, the_mechanical_window_leaves_out_the_current_that_holds_the_cogging)
{
    constexpr uint32_t ticks = 4;
    foc::EstimatorWindowAccumulator accumulator{ hal::Hertz{ 20000 }, ticks };

    for (uint32_t tick = 0; tick != ticks; ++tick)
        accumulator.Accumulate(foc::RotatingFrame{ 0.0f, 1.0f }, foc::RotatingFrame{ 0.0f, 0.0f }, 0.0f, 4.0f, 0.25f);

    const auto snapshot = accumulator.Close();
    EXPECT_NEAR(snapshot.mechanical.meanIq.Value(), 0.75f, 1e-6f);
}
