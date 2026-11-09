#include "core/supervision/I2tThermalModel.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    class I2tThermalModelTest
        : public testing::Test
    {
    public:
        static constexpr supervision::I2tThermalModel::Rating rating{ 5.0f, 10.0f, 2.0f };
        static constexpr float step{ 0.001f };

        supervision::I2tThermalModel model;
    };
}

TEST_F(I2tThermalModelTest, starts_at_zero)
{
    EXPECT_FLOAT_EQ(0.0f, model.State());
}

TEST_F(I2tThermalModelTest, time_constant_reaches_the_continuous_rating_at_the_peak_time)
{
    const auto tau = supervision::I2tThermalModel::TimeConstant(rating);

    EXPECT_NEAR(2.0f / -std::log(0.75f), tau, 1e-4f);
}

TEST_F(I2tThermalModelTest, holding_the_peak_for_the_peak_time_reaches_the_continuous_rating_squared)
{
    for (int i = 0; i < 2000; ++i)
        model.Update(10.0f, rating, step);

    EXPECT_NEAR(25.0f, model.State(), 0.1f);
}

TEST_F(I2tThermalModelTest, a_constant_current_settles_at_its_square)
{
    for (int i = 0; i < 60000; ++i)
        model.Update(3.0f, rating, step);

    EXPECT_NEAR(9.0f, model.State(), 0.05f);
}

TEST_F(I2tThermalModelTest, decays_towards_zero_without_current)
{
    for (int i = 0; i < 2000; ++i)
        model.Update(10.0f, rating, step);
    const auto hot = model.State();

    for (int i = 0; i < 1000; ++i)
        model.Update(0.0f, rating, step);

    EXPECT_LT(model.State(), hot);
    EXPECT_GT(model.State(), 0.0f);
}

TEST_F(I2tThermalModelTest, reset_clears_the_state)
{
    model.Update(10.0f, rating, step);

    model.Reset();

    EXPECT_FLOAT_EQ(0.0f, model.State());
}
