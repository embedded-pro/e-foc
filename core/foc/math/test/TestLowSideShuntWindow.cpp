#include "core/foc/math/LowSideShuntWindow.hpp"
#include <gmock/gmock.h>

namespace
{
    hal::DutyCycle Duty(float fraction)
    {
        return hal::DutyCycle{ static_cast<uint32_t>(fraction * static_cast<float>(hal::DutyCycle::fullScale) + 0.5f) };
    }

    float Fraction(hal::DutyCycle duty)
    {
        return static_cast<float>(duty.Value()) / static_cast<float>(hal::DutyCycle::fullScale);
    }

    class TestLowSideShuntWindow
        : public ::testing::Test
    {
    public:
        const hal::DutyCycle limit{ Duty(0.7f) };
        static constexpr float tolerance{ 1e-4f };
    };
}

TEST_F(TestLowSideShuntWindow, duties_within_the_limit_pass_unchanged)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.7f), Duty(0.5f), Duty(0.1f) }, limit);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.5f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.1f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_duty_above_the_limit_lowers_all_three_and_keeps_the_line_voltages)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.9f), Duty(0.6f), Duty(0.3f) }, limit);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.4f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.1f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_vector_wider_than_the_limit_is_scaled_and_keeps_its_direction)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(1.0f), Duty(0.5f), Duty(0.0f) }, limit);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.35f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_shift_that_reaches_zero_scales_the_rest)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.95f), Duty(0.6f), Duty(0.1f) }, limit);

    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.5f * 0.7f / 0.85f, tolerance);
}

TEST_F(TestLowSideShuntWindow, equal_duties_above_the_limit_move_to_the_limit)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.9f), Duty(0.9f), Duty(0.9f) }, limit);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.7f, tolerance);
}
