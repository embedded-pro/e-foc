#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <gmock/gmock.h>
#include <limits>

namespace
{
    float DutyPercent(hal::DutyCycle duty)
    {
        return 100.0f * foc::DutyFraction(duty);
    }

    struct Modulated
    {
        float a;
        float b;
        float c;
    };

    class TestDutyConversion
        : public ::testing::Test
    {
    public:
        static float Nan()
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

        static float Inf()
        {
            return std::numeric_limits<float>::infinity();
        }
    };
}

TEST_F(TestDutyConversion, a_finite_value_is_a_finite_value)
{
    EXPECT_TRUE(foc::IsFiniteValue(0.0f));
    EXPECT_TRUE(foc::IsFiniteValue(-1.0f));
    EXPECT_TRUE(foc::IsFiniteValue(3.4e38f));
}

TEST_F(TestDutyConversion, nan_and_infinity_are_not_finite)
{
    EXPECT_FALSE(foc::IsFiniteValue(Nan()));
    EXPECT_FALSE(foc::IsFiniteValue(Inf()));
    EXPECT_FALSE(foc::IsFiniteValue(-Inf()));
}

TEST_F(TestDutyConversion, a_normalised_output_maps_onto_the_percent_range)
{
    const auto duties = foc::ToDutyCycles(Modulated{ 0.0f, 0.5f, 1.0f });

    EXPECT_EQ(DutyPercent(duties.a), 0);
    EXPECT_EQ(DutyPercent(duties.b), 50);
    EXPECT_EQ(DutyPercent(duties.c), 100);
}

TEST_F(TestDutyConversion, a_normalised_output_rounds_to_the_nearest_q16_step)
{
    const auto duties = foc::ToDutyCycles(Modulated{ 0.5004f, 0.12345f, 0.9999f });

    EXPECT_EQ(duties.a.Value(), 32794u);
    EXPECT_EQ(duties.b.Value(), 8090u);
    EXPECT_EQ(duties.c.Value(), 65529u);
}

TEST_F(TestDutyConversion, a_normalised_output_keeps_sub_percent_resolution)
{
    const auto duties = foc::ToDutyCycles(Modulated{ 0.5004f, 0.12345f, 0.9999f });

    constexpr float oneStepInPercent = 100.0f / static_cast<float>(hal::DutyCycle::fullScale);
    EXPECT_NEAR(DutyPercent(duties.a), 50.04f, oneStepInPercent);
    EXPECT_NEAR(DutyPercent(duties.b), 12.345f, oneStepInPercent);
    EXPECT_NEAR(DutyPercent(duties.c), 99.99f, oneStepInPercent);
}

TEST_F(TestDutyConversion, an_output_beyond_the_range_is_clamped_rather_than_wrapped)
{
    const auto duties = foc::ToDutyCycles(Modulated{ -5.0f, 1.5f, 0.25f });

    EXPECT_EQ(DutyPercent(duties.a), 0);
    EXPECT_EQ(DutyPercent(duties.b), 100);
    EXPECT_EQ(DutyPercent(duties.c), 25);
}

// Holds only where NaN survives; the embedded build drops this branch under -ffinite-math-only
TEST_F(TestDutyConversion, a_nan_modulation_lands_on_zero_duty)
{
    const auto duties = foc::ToDutyCycles(Modulated{ Nan(), Nan(), Nan() });

    EXPECT_EQ(DutyPercent(duties.a), 0);
    EXPECT_EQ(DutyPercent(duties.b), 0);
    EXPECT_EQ(DutyPercent(duties.c), 0);
}

TEST_F(TestDutyConversion, an_infinite_modulation_saturates_to_a_defined_endpoint)
{
    const auto duties = foc::ToDutyCycles(Modulated{ Inf(), -Inf(), 0.5f });

    EXPECT_EQ(DutyPercent(duties.a), 100);
    EXPECT_EQ(DutyPercent(duties.b), 0);
    EXPECT_EQ(DutyPercent(duties.c), 50);
}
