#include "core/foc/position_loop/PidPositionController.hpp"
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

    foc::PositionLoopTunings TuningsWith(float bandwidth, float integralWeight)
    {
        auto tunings = foc::PositionLoopTunings{};
        tunings.bandwidth = bandwidth;
        tunings.integralWeight = integralWeight;

        return tunings;
    }

    float ProportionalGain()
    {
        return 1.0f / pi;
    }

    float IntegralGain(const foc::PositionLoopTunings& tunings)
    {
        return ProportionalGain() * (tunings.integralWeight / tunings.positionErrorWeight) * samplePeriod * tunings.bandwidth;
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

TEST_F(TestPidPositionController, the_first_sample_applies_the_proportional_and_integral_gains_together)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.1f;

    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(error, 0.0f), (ProportionalGain() + IntegralGain(tunings)) * error * SpeedEnvelope(tunings), tolerance);
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

TEST_F(TestPidPositionController, a_heavier_integral_weight_pushes_harder_on_a_standing_error)
{
    controller.Configure(ValidParameters());
    controller.SetTunings(TuningsWith(foc::PositionLoopTunings{}.bandwidth, 0.5f));
    const float first = Command(0.1f, 0.0f);
    const float second = Command(0.1f, 0.0f);

    foc::PidPositionController gentle;
    gentle.Configure(ValidParameters());
    gentle.SetTunings(TuningsWith(foc::PositionLoopTunings{}.bandwidth, 0.01f));
    const float gentleFirst = gentle.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.1f }, foc::RadiansPerSecond{ 0.0f } }).value;
    const float gentleSecond = gentle.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.1f }, foc::RadiansPerSecond{ 0.0f } }).value;

    EXPECT_GT(second - first, gentleSecond - gentleFirst);
}

TEST_F(TestPidPositionController, a_sustained_error_saturates_at_one_bandwidth_of_speed_reference)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 10; ++sample)
        Command(pi, 0.0f);

    EXPECT_NEAR(Command(pi, 0.0f), SpeedEnvelope(foc::PositionLoopTunings{}), tolerance);
}

TEST_F(TestPidPositionController, a_sustained_negative_error_saturates_symmetrically)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 10; ++sample)
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

    EXPECT_NEAR(Command(-pi + 0.1f, pi - 0.1f), (ProportionalGain() + IntegralGain(tunings)) * 0.2f * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, an_error_across_the_seam_commands_the_short_way_backward)
{
    const auto tunings = foc::PositionLoopTunings{};

    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(pi - 0.1f, -pi + 0.1f), -(ProportionalGain() + IntegralGain(tunings)) * 0.2f * SpeedEnvelope(tunings), tolerance);
}

TEST_F(TestPidPositionController, a_higher_bandwidth_widens_the_speed_envelope)
{
    const auto tunings = TuningsWith(4.0f * foc::PositionLoopTunings{}.bandwidth, foc::PositionLoopTunings{}.integralWeight);

    controller.Configure(ValidParameters());
    controller.SetTunings(tunings);

    EXPECT_NEAR(Command(0.1f, 0.0f), (ProportionalGain() + IntegralGain(tunings)) * 0.1f * SpeedEnvelope(tunings), 1e-3f);
}
