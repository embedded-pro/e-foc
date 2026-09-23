#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <gmock/gmock.h>
#include <limits>

namespace
{
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

    EXPECT_EQ(duties.a.Value(), 0);
    EXPECT_EQ(duties.b.Value(), 50);
    EXPECT_EQ(duties.c.Value(), 100);
}

TEST_F(TestDutyConversion, a_normalised_output_keeps_sub_percent_resolution)
{
    const auto duties = foc::ToDutyCycles(Modulated{ 0.5004f, 0.12345f, 0.9999f });

    EXPECT_NEAR(duties.a.Value(), 50.04f, 1e-4f);
    EXPECT_NEAR(duties.b.Value(), 12.345f, 1e-4f);
    EXPECT_NEAR(duties.c.Value(), 99.99f, 1e-4f);
}

TEST_F(TestDutyConversion, an_output_beyond_the_range_is_clamped_rather_than_wrapped)
{
    const auto duties = foc::ToDutyCycles(Modulated{ -5.0f, 1.5f, 0.25f });

    EXPECT_EQ(duties.a.Value(), 0);
    EXPECT_EQ(duties.b.Value(), 100);
    EXPECT_EQ(duties.c.Value(), 25);
}

// Holds only where NaN survives; the embedded build drops this branch under -ffinite-math-only
TEST_F(TestDutyConversion, a_nan_modulation_lands_on_zero_duty)
{
    const auto duties = foc::ToDutyCycles(Modulated{ Nan(), Nan(), Nan() });

    EXPECT_EQ(duties.a.Value(), 0);
    EXPECT_EQ(duties.b.Value(), 0);
    EXPECT_EQ(duties.c.Value(), 0);
}

TEST_F(TestDutyConversion, an_infinite_modulation_saturates_to_a_defined_endpoint)
{
    const auto duties = foc::ToDutyCycles(Modulated{ Inf(), -Inf(), 0.5f });

    EXPECT_EQ(duties.a.Value(), 100);
    EXPECT_EQ(duties.b.Value(), 0);
    EXPECT_EQ(duties.c.Value(), 50);
}
