#include "core/foc/model/CurrentSense.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

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
            return { foc::M_2310P_LN_04K::parameters, foc::Volts{ 24.0f }, hal::Hertz{ 20000 } };
        }

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

TEST_F(TestCurrentSense, a_settled_amplifier_reads_what_its_exponential_rounds_to)
{
    sense.amplifierTimeConstantSeconds = timeConstant;
    const foc::ConductionWindow window{ 0.0f, 1e-3f };

    for (const auto constants : { 17.0f, 17.9f, 18.0f, 30.0f })
    {
        const auto elapsed = constants * timeConstant;
        EXPECT_EQ(foc::ShuntAmplifierOutput(sense, window, 2.0f, elapsed), 2.0f * (1.0f - std::exp(-elapsed / timeConstant)));
    }
}

TEST_F(TestCurrentSense, a_decayed_amplifier_reads_what_its_exponential_rounds_to)
{
    sense.amplifierTimeConstantSeconds = timeConstant;
    const foc::ConductionWindow window{ -30.0f * timeConstant, 0.0f };

    for (const auto constants : { 90.0f, 103.0f, 104.0f, 200.0f })
    {
        const auto elapsed = constants * timeConstant;
        EXPECT_EQ(foc::ShuntAmplifierOutput(sense, window, 2.0f, elapsed), 2.0f * std::exp(-elapsed / timeConstant));
    }
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
    const foc::CurrentSenseConfig chain{ 16e6f, 16, 12, 2, 0.0f, 0.0f, 0.0f, 0 };
    sensed.CurrentSensor().SetChain(chain);
    const foc::PhasePwmDutyCycles duties{ hal::DutyCycle::FromPercent(75), hal::DutyCycle::FromPercent(70), hal::DutyCycle::FromPercent(65) };

    for (auto cycle = 0; cycle != 20; ++cycle)
    {
        ideal.StepForTest(duties);
        sensed.StepForTest(duties);
    }

    const auto actual = ideal.CurrentSensor().Last();
    const auto measured = sensed.CurrentSensor().Last();
    const auto slopes = sensed.Electrical().ZeroVectorSlopes(sensed.Mechanics().ElectricalAngle(), sensed.Mechanics().ElectricalSpeed(), sensed.Winding());
    const auto at = [&chain](uint32_t conversion)
    {
        return foc::ConversionHoldInstant(chain, conversion);
    };

    ASSERT_GT(actual.a.Value(), 0.1f);
    EXPECT_NEAR(measured.a.Value(), actual.a.Value() + slopes.a.Value() * 0.5f * (at(0) + at(1)), 1e-5f);
    EXPECT_NEAR(measured.b.Value(), actual.b.Value() + slopes.b.Value() * 0.5f * (at(2) + at(3)), 1e-5f);
    EXPECT_NEAR(measured.c.Value(), 0.5f * (actual.c.Value() + slopes.c.Value() * at(4)), 1e-5f);
}

TEST_F(TestCurrentSense, a_phase_is_read_where_its_zero_vector_slope_has_taken_it_by_the_hold_instants)
{
    auto sample = Sample(0.5f, 2.0f);
    sample.slope = 1.0e5f;

    EXPECT_NEAR(foc::ShuntReading(sense, sample, 0, quiet), 2.0f + 1.0e5f * 0.75e-6f, 1e-5f);
    EXPECT_NEAR(foc::ShuntReading(sense, sample, 1, quiet), 2.0f + 1.0e5f * 2.75e-6f, 1e-5f);
    EXPECT_NEAR(foc::ShuntReading(sense, sample, 2, quiet), 2.0f + 1.0e5f * 4.75e-6f, 1e-5f);
}

TEST_F(TestMotorModelCurrentSense, at_speed_without_current_the_back_emf_alone_sets_the_slopes)
{
    auto model = Model();
    const auto winding = model.Winding();
    constexpr float omega{ 400.0f };

    for (const auto theta : { 0.0f, 1.0f, -2.0f })
    {
        const auto slopes = model.Electrical().ZeroVectorSlopes(foc::Radians{ theta }, foc::RadiansPerSecond{ omega }, winding);
        const auto rate = omega * winding.fluxLinkage / winding.inductanceQ;

        EXPECT_NEAR(slopes.a.Value(), rate * std::sin(theta), 1e-3f * rate);
        EXPECT_NEAR(slopes.b.Value(), rate * std::sin(theta - 2.0f * std::numbers::pi_v<float> / 3.0f), 1e-3f * rate);
        EXPECT_NEAR(slopes.c.Value(), rate * std::sin(theta + 2.0f * std::numbers::pi_v<float> / 3.0f), 1e-3f * rate);
    }
}
