#include "core/foc/position_loop/LqiPositionController.hpp"
#include <algorithm>
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float tolerance = 1e-4f;
    constexpr float inertia = 0.001f;
    constexpr float viscousFriction = 0.0001f;
    constexpr float torqueConstant = 0.1f;
    constexpr float maxCurrent = 10.0f;
    constexpr uint32_t samplingFrequency = 1000;
    constexpr float samplePeriod = 1.0f / static_cast<float>(samplingFrequency);

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ inertia }, foc::NewtonMeterSecondPerRadian{ viscousFriction },
            foc::NewtonMeter{ torqueConstant }, foc::Ampere{ maxCurrent }, hal::Hertz{ samplingFrequency } };
    }

    float CurrentPerNormalizedInput()
    {
        return inertia / (torqueConstant * samplePeriod * samplePeriod);
    }

    foc::LqiPositionController::Design DesignedByHand(const foc::PositionLoopTunings& tunings)
    {
        using Design = foc::LqiPositionController::Design;

        const float speedDecay = 1.0f - viscousFriction * samplePeriod / inertia;
        const float bandwidthPerSample = std::clamp(tunings.bandwidth * samplePeriod, 1e-3f, 0.5f);

        const Design::StateMatrix a{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, speedDecay } };
        const Design::InputMatrix b{ { 0.0f }, { 0.0f }, { 1.0f } };
        const Design::StateMatrix q{
            { tunings.integralWeight / tunings.positionErrorWeight, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, tunings.speedErrorWeight / tunings.positionErrorWeight }
        };
        const Design::InputWeightMatrix r{ 1.0f / (bandwidthPerSample * bandwidthPerSample) };

        return *Design::TryCreate(a, b, q, r);
    }

    float ExpectedCurrent(const foc::PositionLoopTunings& tunings, float accumulatedDeviation, float error, float speed)
    {
        auto design = DesignedByHand(tunings);
        const auto command = design.ComputeControl(
            foc::LqiPositionController::Design::StateVector{ accumulatedDeviation, -error, speed * samplePeriod });

        return command.at(0, 0) * CurrentPerNormalizedInput();
    }

    class TestLqiPositionController
        : public ::testing::Test
    {
    public:
        float Command(float reference, float measured, float speed = 0.0f)
        {
            return controller.Compute({ foc::Radians{ measured }, foc::Radians{ reference }, foc::RadiansPerSecond{ speed } }).value;
        }

        float SettledPosition(float reference, float loadTorque)
        {
            float position = 0.0f;
            float speed = 0.0f;

            for (int sample = 0; sample != 20000; ++sample)
            {
                const auto current = Command(reference, position, speed);
                speed += (torqueConstant * current - loadTorque - viscousFriction * speed) / inertia * samplePeriod;
                position += speed * samplePeriod;
            }

            return position;
        }

        foc::LqiPositionController controller;
    };
}

TEST_F(TestLqiPositionController, unconfigured_controller_outputs_zero)
{
    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestLqiPositionController, a_zero_inertia_leaves_the_loop_inert)
{
    auto parameters = ValidParameters();
    parameters.inertia = foc::NewtonMeterSecondSquared{ 0.0f };

    controller.Configure(ValidParameters());
    controller.Configure(parameters);

    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestLqiPositionController, the_command_is_a_current_reference)
{
    controller.Configure(ValidParameters());

    EXPECT_EQ(controller.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.0001f }, foc::RadiansPerSecond{ 0.0f } }).kind,
        foc::PositionOutputKind::currentReference);
}

TEST_F(TestLqiPositionController, the_first_sample_feeds_one_deviation_into_the_integral_state)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.0001f;

    controller.Configure(ValidParameters());

    const float expected = ExpectedCurrent(tunings, -error, error, 0.0f);
    ASSERT_LT(std::abs(expected), maxCurrent);
    EXPECT_NEAR(Command(error, 0.0f), expected, tolerance);
}

TEST_F(TestLqiPositionController, the_second_sample_carries_two_deviations_in_the_integral_state)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.0001f;

    controller.Configure(ValidParameters());
    Command(error, 0.0f);

    const float expected = ExpectedCurrent(tunings, -2.0f * error, error, 0.0f);
    ASSERT_LT(std::abs(expected), maxCurrent);
    EXPECT_NEAR(Command(error, 0.0f), expected, tolerance);
}

TEST_F(TestLqiPositionController, a_standing_error_keeps_building_the_command)
{
    controller.Configure(ValidParameters());

    const float first = Command(0.0001f, 0.0f);

    EXPECT_GT(Command(0.0001f, 0.0f), first);
}

TEST_F(TestLqiPositionController, a_matched_position_at_standstill_commands_no_current)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(0.5f, 0.5f), 0.0f, tolerance);
}

TEST_F(TestLqiPositionController, a_large_error_saturates_at_the_current_envelope)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(3.0f, 0.0f), maxCurrent, tolerance);

    controller.Reset();

    EXPECT_NEAR(Command(-3.0f, 0.0f), -maxCurrent, tolerance);
}

TEST_F(TestLqiPositionController, sustained_saturation_does_not_wind_up_the_integral_state)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 200; ++sample)
        Command(3.0f, 0.0f);

    EXPECT_NEAR(Command(0.5f, 0.5f), 0.0f, tolerance);
}

TEST_F(TestLqiPositionController, reset_clears_the_integral_state)
{
    controller.Configure(ValidParameters());
    const float first = Command(0.0001f, 0.0f);
    Command(0.0001f, 0.0f);

    controller.Reset();

    EXPECT_NEAR(Command(0.0001f, 0.0f), first, tolerance);
}

TEST_F(TestLqiPositionController, retuning_clears_the_integral_state)
{
    controller.Configure(ValidParameters());
    const float first = Command(0.0001f, 0.0f);

    controller.SetTunings(foc::PositionLoopTunings{});

    EXPECT_NEAR(Command(0.0001f, 0.0f), first, tolerance);
}

TEST_F(TestLqiPositionController, an_error_across_the_seam_drives_the_short_way_forward)
{
    controller.Configure(ValidParameters());
    const float acrossTheSeam = Command(-pi + 0.00005f, pi - 0.00005f);

    controller.Reset();

    EXPECT_NEAR(acrossTheSeam, Command(0.0001f, 0.0f), tolerance);
}

TEST_F(TestLqiPositionController, an_error_across_the_seam_drives_the_short_way_backward)
{
    controller.Configure(ValidParameters());
    const float acrossTheSeam = Command(pi - 0.00005f, -pi + 0.00005f);

    controller.Reset();

    EXPECT_NEAR(acrossTheSeam, Command(-0.0001f, 0.0f), tolerance);
}

TEST_F(TestLqiPositionController, the_integral_state_removes_the_standing_error_of_a_constant_load)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(SettledPosition(0.5f, 0.1f), 0.5f, 1e-3f);
}

TEST_F(TestLqiPositionController, an_inert_design_commands_zero_however_long_the_error_stands)
{
    // The saturation guard that unwinds the accumulation compares the limited command against the
    // unlimited one, and with a zero gain matrix both are zero, so it never fires. What the loop
    // must not do either way is command anything: the design has no actuator mapping at all.
    auto parameters = ValidParameters();
    parameters.inertia = foc::NewtonMeterSecondSquared{ 0.0f };
    controller.Configure(parameters);

    for (int sample = 0; sample != 200000; ++sample)
    {
        const auto command = Command(3.0f, 0.0f);
        ASSERT_TRUE(std::isfinite(command)) << " at sample " << sample;
        ASSERT_NEAR(command, 0.0f, tolerance) << " at sample " << sample;
    }
}

TEST_F(TestLqiPositionController, a_design_installed_after_an_inert_run_starts_from_a_clear_integral_state)
{
    auto inert = ValidParameters();
    inert.inertia = foc::NewtonMeterSecondSquared{ 0.0f };
    controller.Configure(inert);

    for (int sample = 0; sample != 200000; ++sample)
        Command(3.0f, 0.0f);

    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(0.0f, 0.0f), 0.0f, tolerance);
}
