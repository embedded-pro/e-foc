#include "core/foc/position_loop/PidPositionController.hpp"
#include <algorithm>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float tolerance = 1e-4f;
    constexpr uint32_t samplingFrequency = 1000;
    constexpr float samplePeriod = 1.0f / static_cast<float>(samplingFrequency);

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ samplingFrequency } };
    }

    foc::PositionLoopTunings TuningsWith(float bandwidth)
    {
        auto tunings = foc::PositionLoopTunings{};
        tunings.bandwidth = bandwidth;

        return tunings;
    }

    float ProportionalGain()
    {
        return 1.0f / pi;
    }

    float IntegralGain(const foc::PositionLoopTunings& tunings)
    {
        return ProportionalGain() * foc::PidPositionController::integralZeroRatio * tunings.bandwidth * samplePeriod;
    }

    float ReferenceStepGain(const foc::PositionLoopTunings& tunings)
    {
        return ProportionalGain() * foc::PidPositionController::referenceWeight + IntegralGain(tunings);
    }

    float SpeedEnvelope(const foc::PositionLoopTunings& tunings)
    {
        return tunings.bandwidth * pi;
    }

    class TestPidPositionController
        : public ::testing::Test
    {
    public:
        float Command(float reference, float measured)
        {
            return controller.Compute({ foc::Radians{ measured }, foc::Radians{ reference }, foc::RadiansPerSecond{ 0.0f } }).value;
        }

        float PositionErrorAfterStep(float step, float disturbanceSpeed, std::size_t samples)
        {
            constexpr float speedLoopBandwidth = 100.0f;
            float position{ 0.0f };
            float speed{ 0.0f };

            for (std::size_t sample = 0; sample != samples; ++sample)
            {
                const auto reference = Command(step, position);
                speed += (reference - speed) * speedLoopBandwidth * samplePeriod;
                position += (speed + disturbanceSpeed) * samplePeriod;
            }

            return step - position;
        }

        float PeakOvershootAfterStep(float step, std::size_t samples)
        {
            constexpr float speedLoopBandwidth = 100.0f;
            float position{ 0.0f };
            float speed{ 0.0f };
            float peak{ 0.0f };

            for (std::size_t sample = 0; sample != samples; ++sample)
            {
                const auto reference = Command(step, position);
                speed += (reference - speed) * speedLoopBandwidth * samplePeriod;
                position += speed * samplePeriod;
                peak = std::max(peak, position);
            }

            return peak / step - 1.0f;
        }

        foc::PidPositionController controller;
    };
}

TEST_F(TestPidPositionController, unconfigured_controller_outputs_zero)
{
    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestPidPositionController, a_zero_sampling_frequency_leaves_the_loop_silent)
{
    auto parameters = ValidParameters();
    parameters.samplingFrequency = hal::Hertz{ 0 };
    controller.Configure(parameters);

    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestPidPositionController, the_command_is_a_speed_reference)
{
    controller.Configure(ValidParameters());

    EXPECT_EQ(controller.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.1f }, foc::RadiansPerSecond{ 0.0f } }).kind,
        foc::PositionOutputKind::speedReference);
}

TEST_F(TestPidPositionController, the_first_sample_treats_the_error_as_a_reference_step)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.1f;

    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(error, 0.0f), ReferenceStepGain(tunings) * error * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, a_measured_move_is_met_with_the_full_proportional_gain)
{
    const auto tunings = foc::PositionLoopTunings{};

    controller.Configure(ValidParameters());
    const float settled = Command(0.0f, 0.0f);
    const float pushed = Command(0.0f, 0.1f);

    EXPECT_NEAR(pushed - settled, -(ProportionalGain() + IntegralGain(tunings)) * 0.1f * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, a_step_settles_without_a_slow_tail)
{
    controller.Configure(ValidParameters());

    EXPECT_LT(std::abs(PositionErrorAfterStep(1.5f, 0.0f, 450)), 0.02f);
}

TEST_F(TestPidPositionController, a_step_overshoots_by_less_than_five_percent)
{
    controller.Configure(ValidParameters());

    EXPECT_LT(PeakOvershootAfterStep(1.5f, 1000), 0.05f);
}

TEST_F(TestPidPositionController, a_constant_disturbance_speed_is_integrated_away)
{
    controller.Configure(ValidParameters());

    EXPECT_LT(std::abs(PositionErrorAfterStep(1.5f, 0.5f, 2000)), 1e-3f);
}

TEST_F(TestPidPositionController, a_standing_error_adds_exactly_one_integral_step_per_sample)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.1f;

    controller.Configure(ValidParameters());
    const float first = Command(error, 0.0f);
    const float second = Command(error, 0.0f);

    EXPECT_NEAR(second - first, IntegralGain(tunings) * error * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, a_sustained_error_saturates_at_one_bandwidth_of_speed_reference)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 100; ++sample)
        Command(pi, 0.0f);

    EXPECT_NEAR(Command(pi, 0.0f), SpeedEnvelope(foc::PositionLoopTunings{}), tolerance);
}

TEST_F(TestPidPositionController, a_sustained_negative_error_saturates_symmetrically)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 100; ++sample)
        Command(-pi, 0.0f);

    EXPECT_NEAR(Command(-pi, 0.0f), -SpeedEnvelope(foc::PositionLoopTunings{}), tolerance);
}

TEST_F(TestPidPositionController, reset_clears_the_accumulated_command)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 10; ++sample)
        Command(pi, 0.0f);

    controller.Reset();

    EXPECT_NEAR(Command(0.5f, 0.5f), 0.0f, tolerance);
}

TEST_F(TestPidPositionController, an_error_across_the_seam_commands_the_short_way_forward)
{
    const auto tunings = foc::PositionLoopTunings{};

    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(-pi + 0.1f, pi - 0.1f), ReferenceStepGain(tunings) * 0.2f * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, an_error_across_the_seam_commands_the_short_way_backward)
{
    const auto tunings = foc::PositionLoopTunings{};

    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(pi - 0.1f, -pi + 0.1f), -ReferenceStepGain(tunings) * 0.2f * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, a_higher_bandwidth_widens_the_speed_envelope)
{
    const auto tunings = TuningsWith(4.0f * foc::PositionLoopTunings{}.bandwidth);

    controller.Configure(ValidParameters());
    controller.SetTunings(tunings);

    EXPECT_NEAR(Command(0.1f, 0.0f), ReferenceStepGain(tunings) * 0.1f * SpeedEnvelope(tunings), 1e-3f);
}
