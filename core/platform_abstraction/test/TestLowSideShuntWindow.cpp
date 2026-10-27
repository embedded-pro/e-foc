#include "core/platform_abstraction/LowSideShuntWindow.hpp"
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
        const hal::DutyCycle maxDuty{ Duty(1.0f) };
        static constexpr float tolerance{ 1e-4f };
    };
}

TEST_F(TestLowSideShuntWindow, duties_within_the_limit_pass_unchanged)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.7f), Duty(0.5f), Duty(0.1f) }, limit, maxDuty);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.5f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.1f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_duty_above_the_limit_lowers_all_three_and_keeps_the_line_voltages)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.9f), Duty(0.6f), Duty(0.3f) }, limit, maxDuty);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.4f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.1f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_vector_wider_than_the_limit_keeps_its_highest_duty_when_the_middle_one_is_readable)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(1.0f), Duty(0.5f), Duty(0.0f) }, limit, maxDuty);

    EXPECT_NEAR(Fraction(kept.a), 1.0f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.5f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_shift_that_reaches_zero_leaves_the_highest_duty_above_the_limit)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.95f), Duty(0.6f), Duty(0.1f) }, limit, maxDuty);

    EXPECT_NEAR(Fraction(kept.a), 0.85f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.5f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_middle_duty_above_the_limit_scales_the_vector_and_keeps_its_direction)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(1.0f), Duty(0.9f), Duty(0.0f) }, limit, maxDuty);

    EXPECT_NEAR(Fraction(kept.a), 0.7f / 0.9f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
}

TEST_F(TestLowSideShuntWindow, a_highest_duty_above_the_maximum_scales_the_vector_and_keeps_its_direction)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(1.0f), Duty(0.5f), Duty(0.0f) }, limit, Duty(0.95f));

    EXPECT_NEAR(Fraction(kept.a), 0.95f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.475f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
}

TEST_F(TestLowSideShuntWindow, equal_duties_above_the_limit_move_to_the_limit)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(0.9f), Duty(0.9f), Duty(0.9f) }, limit, maxDuty);

    EXPECT_NEAR(Fraction(kept.a), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.7f, tolerance);
}

namespace
{
    class TestLowSideShuntReadout
        : public TestLowSideShuntWindow
    {
    public:
        foc::LowSideShuntReadout readout{ limit };
        const foc::PhaseCurrents measured{ foc::Ampere{ 0.9f }, foc::Ampere{ -0.4f }, foc::Ampere{ -0.3f } };
    };

    void ExpectCurrents(const foc::PhaseCurrents& actual, float a, float b, float c)
    {
        EXPECT_NEAR(actual.a.Value(), a, 1e-6f);
        EXPECT_NEAR(actual.b.Value(), b, 1e-6f);
        EXPECT_NEAR(actual.c.Value(), c, 1e-6f);
    }
}

TEST_F(TestLowSideShuntReadout, readable_duties_pass_all_three_currents)
{
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.9f, -0.4f, -0.3f);
}

TEST_F(TestLowSideShuntReadout, the_phase_above_the_limit_follows_from_the_other_two)
{
    readout.Applied({ Duty(0.5f), Duty(0.9f), Duty(0.0f) });
    readout.Applied({ Duty(0.5f), Duty(0.9f), Duty(0.0f) });

    ExpectCurrents(readout.Read(measured), 0.9f, -0.6f, -0.3f);
}

TEST_F(TestLowSideShuntReadout, each_phase_can_be_the_one_rebuilt)
{
    readout.Applied({ Duty(0.9f), Duty(0.5f), Duty(0.0f) });
    readout.Applied({ Duty(0.9f), Duty(0.5f), Duty(0.0f) });
    ExpectCurrents(readout.Read(measured), 0.7f, -0.4f, -0.3f);

    readout.Applied({ Duty(0.0f), Duty(0.5f), Duty(0.9f) });
    readout.Applied({ Duty(0.0f), Duty(0.5f), Duty(0.9f) });
    ExpectCurrents(readout.Read(measured), 0.9f, -0.4f, -0.5f);
}

TEST_F(TestLowSideShuntReadout, a_duty_above_the_limit_in_the_period_before_still_hides_the_phase)
{
    readout.Applied({ Duty(0.9f), Duty(0.5f), Duty(0.0f) });
    readout.Applied({ Duty(0.6f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.7f, -0.4f, -0.3f);
}

TEST_F(TestLowSideShuntReadout, two_readable_periods_after_one_above_the_limit_pass_all_three_again)
{
    readout.Applied({ Duty(0.9f), Duty(0.5f), Duty(0.0f) });
    readout.Applied({ Duty(0.6f), Duty(0.5f), Duty(0.1f) });
    readout.Applied({ Duty(0.6f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.9f, -0.4f, -0.3f);
}
