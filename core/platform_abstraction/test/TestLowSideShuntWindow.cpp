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

TEST_F(TestLowSideShuntWindow, with_both_bounds_exceeded_the_middle_one_scales_when_it_is_the_tighter)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(1.0f), Duty(0.9f), Duty(0.0f) }, limit, Duty(0.95f));

    EXPECT_NEAR(Fraction(kept.a), 0.7f / 0.9f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.7f, tolerance);
    EXPECT_NEAR(Fraction(kept.c), 0.0f, tolerance);
}

TEST_F(TestLowSideShuntWindow, with_both_bounds_exceeded_the_maximum_scales_when_it_is_the_tighter)
{
    const auto kept = foc::KeepLowSideShuntsReadable({ Duty(1.0f), Duty(0.75f), Duty(0.0f) }, limit, Duty(0.8f));

    EXPECT_NEAR(Fraction(kept.a), 0.8f, tolerance);
    EXPECT_NEAR(Fraction(kept.b), 0.6f, tolerance);
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

TEST_F(TestLowSideShuntReadout, a_rebuilt_phase_carries_the_offsets_so_removing_each_phase_offset_leaves_its_current)
{
    const foc::PhaseCurrents offsets{ foc::Ampere{ 0.027f }, foc::Ampere{ -0.018f }, foc::Ampere{ 0.054f } };
    readout.Offsets(offsets);
    readout.Applied({ Duty(0.5f), Duty(0.9f), Duty(0.0f) });
    readout.Applied({ Duty(0.5f), Duty(0.9f), Duty(0.0f) });

    const auto rebuilt = readout.Read({ foc::Ampere{ 0.9f + 0.027f }, foc::Ampere{ 5.0f }, foc::Ampere{ -0.3f + 0.054f } });

    ExpectCurrents({ rebuilt.a - offsets.a, rebuilt.b - offsets.b, rebuilt.c - offsets.c }, 0.9f, -0.6f, -0.3f);
}

TEST_F(TestLowSideShuntReadout, readable_phases_pass_unchanged_whatever_the_offsets)
{
    readout.Offsets({ foc::Ampere{ 0.027f }, foc::Ampere{ -0.018f }, foc::Ampere{ 0.054f } });
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.9f, -0.4f, -0.3f);
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

namespace
{
    class TestLowSideShuntReadoutSkew
        : public TestLowSideShuntReadout
    {
    public:
        TestLowSideShuntReadoutSkew()
        {
            readout.SampleDelays(delays);
        }

        using Seconds = std::chrono::duration<float>;

        const foc::PhaseSampleDelays delays{ Seconds{ 0.75e-6f }, Seconds{ 2.75e-6f }, Seconds{ 4.75e-6f } };
        const foc::PhaseCurrentSlopes slopes{ foc::AmperePerSecond{ 40000.0f }, foc::AmperePerSecond{ -20000.0f }, foc::AmperePerSecond{ -20000.0f } };
    };
}

TEST_F(TestLowSideShuntReadoutSkew, without_slopes_the_samples_pass_unchanged)
{
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.9f, -0.4f, -0.3f);
}

TEST_F(TestLowSideShuntReadoutSkew, each_phase_read_is_brought_back_by_its_slope_times_its_delay)
{
    readout.ZeroVectorSlopes(slopes);
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.9f - 0.03f, -0.4f + 0.055f, -0.3f + 0.095f);
}

TEST_F(TestLowSideShuntReadoutSkew, a_hidden_phase_is_rebuilt_from_the_two_corrected_phases)
{
    readout.ZeroVectorSlopes(slopes);
    readout.Applied({ Duty(0.5f), Duty(0.5f), Duty(0.9f) });
    readout.Applied({ Duty(0.5f), Duty(0.5f), Duty(0.9f) });

    const auto corrected = readout.Read({ foc::Ampere{ 0.9f }, foc::Ampere{ -0.4f }, foc::Ampere{ 7.0f } });

    ExpectCurrents(corrected, 0.87f, -0.345f, -(0.87f - 0.345f));
}

TEST_F(TestLowSideShuntReadoutSkew, cleared_slopes_leave_the_samples_unchanged)
{
    readout.ZeroVectorSlopes(slopes);
    readout.ClearSlopes();
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });
    readout.Applied({ Duty(0.7f), Duty(0.5f), Duty(0.1f) });

    ExpectCurrents(readout.Read(measured), 0.9f, -0.4f, -0.3f);
}

TEST(TestSequentialPhaseSampleDelays, each_phase_is_held_at_the_mean_of_its_averaged_conversions)
{
    const auto delays = foc::SequentialPhaseSampleDelays({ .converterClockHertz = 16.0e6f, .sampleHoldClocks = 4, .conversionClocks = 12, .averagedConversions = 2, .triggerDelay = std::chrono::duration<float>{ 0.0f } });

    EXPECT_NEAR(delays.a.count(), 0.75e-6f, 1e-10f);
    EXPECT_NEAR(delays.b.count(), 2.75e-6f, 1e-10f);
    EXPECT_NEAR(delays.c.count(), 4.75e-6f, 1e-10f);
}

TEST(TestSequentialPhaseSampleDelays, the_trigger_delay_adds_to_every_phase_and_one_conversion_needs_no_averaging)
{
    const auto delays = foc::SequentialPhaseSampleDelays({ .converterClockHertz = 16.0e6f, .sampleHoldClocks = 16, .conversionClocks = 12, .averagedConversions = 1, .triggerDelay = std::chrono::duration<float>{ 1.0e-6f } });

    EXPECT_NEAR(delays.a.count(), 1.0e-6f + 1.0e-6f, 1e-10f);
    EXPECT_NEAR(delays.b.count(), 1.0e-6f + 2.75e-6f, 1e-10f);
    EXPECT_NEAR(delays.c.count(), 1.0e-6f + 4.5e-6f, 1e-10f);
}

TEST(TestSequentialPhaseSampleDelays, no_converter_clock_gives_no_delay)
{
    const auto delays = foc::SequentialPhaseSampleDelays({});

    EXPECT_EQ(delays.a.count(), 0.0f);
    EXPECT_EQ(delays.b.count(), 0.0f);
    EXPECT_EQ(delays.c.count(), 0.0f);
}
