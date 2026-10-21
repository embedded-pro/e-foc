#include "core/foc/model/CurrentSense.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    constexpr float period{ 50e-6f };
    constexpr float timeConstant{ 200e-9f };

    class TestCurrentSense
        : public ::testing::Test
    {
    protected:
        foc::CurrentSenseConfig sense{ 16e6f, 4, 12, 2, 0.0f, 0.0f, 0.0f, 0 };
        infra::Function<float()> quiet{ []()
            {
                return 0.0f;
            } };

        foc::ShuntSample Sample(float duty, float current, float offset = 0.0f) const
        {
            return { foc::LowSideConduction({}, duty, current, period), current, offset };
        }
    };

    class TestMotorModelCurrentSense
        : public ::testing::Test
    {
    protected:
        foc::ThreePhaseMotorModel Model() const
        {
            return { foc::M_2310P_LN_04K::parameters, foc::Volts{ 24.0f }, hal::Hertz{ 20000 }, std::optional<std::size_t>{} };
        }

        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;
        foc::ThreePhaseMotorModel ideal{ Model() };
        foc::ThreePhaseMotorModel sensed{ Model() };
    };
}

TEST_F(TestCurrentSense, a_sequence_holds_each_conversion_one_conversion_after_the_last)
{
    sense.triggerDelaySeconds = 1e-6f;

    EXPECT_NEAR(foc::ConversionHoldInstant(sense, 0), 1.25e-6f, 1e-9f);
    EXPECT_NEAR(foc::ConversionHoldInstant(sense, 5), 6.25e-6f, 1e-9f);
}

TEST_F(TestCurrentSense, inside_its_window_the_shunt_reads_its_phase_and_the_amplifier_offset)
{
    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, 2.0f, 0.03f), 2, quiet), 2.03f, 1e-6f);
}

TEST_F(TestCurrentSense, outside_its_window_the_shunt_reads_only_the_offset)
{
    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.9f, 2.0f, 0.03f), 2, quiet), 0.03f, 1e-6f);
}

TEST_F(TestCurrentSense, a_window_closing_between_two_averaged_conversions_halves_the_reading)
{
    const auto duty = 1.0f - 2.0f * 5.0e-6f / period;

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(duty, 2.0f), 1, quiet), 2.0f, 1e-6f);
    EXPECT_NEAR(foc::ShuntReading(sense, Sample(duty, 2.0f), 2, quiet), 1.0f, 1e-6f);
}

TEST_F(TestCurrentSense, a_longer_sample_and_hold_moves_the_last_phase_out_of_a_window_the_shortest_one_reads)
{
    const auto duty = 0.7f;
    sense.sampleHoldClocks = 16;

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(duty, 2.0f), 2, quiet), 0.0f, 1e-6f);

    sense.sampleHoldClocks = 4;

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(duty, 2.0f), 2, quiet), 2.0f, 1e-6f);
}

TEST_F(TestCurrentSense, the_amplifier_settles_into_the_window_and_decays_after_it)
{
    sense.amplifierTimeConstantSeconds = timeConstant;
    const foc::ConductionWindow window{ 0.0f, 1e-6f };

    EXPECT_NEAR(foc::ShuntAmplifierOutput(sense, window, 2.0f, timeConstant), 2.0f * (1.0f - std::exp(-1.0f)), 1e-5f);
    EXPECT_NEAR(foc::ShuntAmplifierOutput(sense, window, 2.0f, window.end + timeConstant), 2.0f * (1.0f - std::exp(-5.0f)) * std::exp(-1.0f), 1e-5f);
    EXPECT_NEAR(foc::ShuntAmplifierOutput(sense, window, 2.0f, -1e-9f), 0.0f, 1e-9f);
}

TEST_F(TestCurrentSense, the_converter_rounds_each_conversion_to_a_code_and_truncates_their_mean)
{
    sense.fullScaleAmpere = 8.0f;
    sense.resolutionBits = 4;
    auto toggle = false;
    const infra::Function<float()> alternating{ [&toggle]()
        {
            toggle = !toggle;
            return toggle ? 0.0f : 1.0f;
        } };

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, 1.2f), 0, quiet), 1.0f, 1e-6f);
    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, 1.2f), 0, alternating), 1.0f, 1e-6f);
}

TEST_F(TestCurrentSense, the_converter_clips_at_its_full_scale)
{
    sense.fullScaleAmpere = 8.0f;

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, 12.0f), 0, quiet), 8.0f, 1e-6f);

    sense.resolutionBits = 12;

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, -12.0f), 0, quiet), -8.0f, 1e-6f);
    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, 12.0f), 0, quiet), 8.0f - 16.0f / 4096.0f, 1e-6f);
}

TEST_F(TestCurrentSense, each_conversion_draws_its_own_noise_and_the_step_averages_it)
{
    sense.averagedConversions = 4;
    auto index = 0;
    const infra::Function<float()> pattern{ [&index]()
        {
            return (index++ % 2 == 0) ? 0.1f : -0.1f;
        } };

    EXPECT_NEAR(foc::ShuntReading(sense, Sample(0.5f, 1.0f), 0, pattern), 1.0f, 1e-6f);
}

TEST_F(TestMotorModelCurrentSense, the_model_converts_the_phases_in_order_inside_the_windows_of_their_own_duties)
{
    sensed.SetCurrentSense({ 16e6f, 16, 12, 2, 0.0f, 0.0f, 0.0f, 0 });
    const foc::PhasePwmDutyCycles duties{ hal::DutyCycle::FromPercent(75), hal::DutyCycle::FromPercent(70), hal::DutyCycle::FromPercent(65) };

    for (auto cycle = 0; cycle != 20; ++cycle)
    {
        ideal.StepForTest(duties);
        sensed.StepForTest(duties);
    }

    const auto actual = ideal.LastMeasuredCurrents();
    const auto measured = sensed.LastMeasuredCurrents();

    ASSERT_GT(actual.a.Value(), 0.1f);
    EXPECT_NEAR(measured.a.Value(), actual.a.Value(), 1e-6f);
    EXPECT_NEAR(measured.b.Value(), actual.b.Value(), 1e-6f);
    EXPECT_NEAR(measured.c.Value(), 0.5f * actual.c.Value(), 1e-6f);
}
