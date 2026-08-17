#include "core/foc/position_loop/LqrPositionController.hpp"
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

    // The normalised input absorbs (Kt / J) * Ts^2, so one unit of it costs J / (Kt * Ts^2) Amperes
    float CurrentPerNormalizedInput()
    {
        return inertia / (torqueConstant * samplePeriod * samplePeriod);
    }

    foc::LqrPositionController::Design DesignedByHand(const foc::PositionLoopTunings& tunings)
    {
        using Design = foc::LqrPositionController::Design;

        const float speedDecay = 1.0f - viscousFriction * samplePeriod / inertia;
        const float bandwidthPerSample = std::clamp(tunings.bandwidth * samplePeriod, 1e-3f, 0.5f);

        const Design::StateMatrix a{ { 1.0f, 1.0f }, { 0.0f, speedDecay } };
        const Design::InputMatrix b{ { 0.0f }, { 1.0f } };
        const Design::StateMatrix q{ { 1.0f, 0.0f }, { 0.0f, tunings.speedErrorWeight / tunings.positionErrorWeight } };
        const Design::InputWeightMatrix r{ 1.0f / (bandwidthPerSample * bandwidthPerSample) };

        return *Design::TryCreate(a, b, q, r);
    }

    float ExpectedCurrent(const foc::PositionLoopTunings& tunings, float error, float speed)
    {
        auto design = DesignedByHand(tunings);
        const auto command = design.ComputeControl(foc::LqrPositionController::Design::StateVector{ -error, speed * samplePeriod });

        return command.at(0, 0) * CurrentPerNormalizedInput();
    }

    class TestLqrPositionController
        : public ::testing::Test
    {
    public:
        float Command(float reference, float measured, float speed = 0.0f)
        {
            return controller.Compute({ foc::Radians{ measured }, foc::Radians{ reference }, foc::RadiansPerSecond{ speed } }).value;
        }

        // Rigid body integrated with the same rectangular rule the discretisation assumes
        float SettledPosition(float reference)
        {
            float position = 0.0f;
            float speed = 0.0f;

            for (int sample = 0; sample != 4000; ++sample)
            {
                const auto current = Command(reference, position, speed);
                speed += (torqueConstant * current - viscousFriction * speed) / inertia * samplePeriod;
                position += speed * samplePeriod;
            }

            return position;
        }

        foc::LqrPositionController controller;
    };
}

TEST_F(TestLqrPositionController, unconfigured_controller_outputs_zero)
{
    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestLqrPositionController, a_zero_inertia_leaves_the_loop_inert)
{
    auto parameters = ValidParameters();
    parameters.inertia = foc::NewtonMeterSecondSquared{ 0.0f };

    controller.Configure(ValidParameters());
    controller.Configure(parameters);

    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestLqrPositionController, a_zero_torque_constant_leaves_the_loop_inert)
{
    auto parameters = ValidParameters();
    parameters.torqueConstant = foc::NewtonMeter{ 0.0f };

    controller.Configure(parameters);

    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestLqrPositionController, the_command_is_a_current_reference)
{
    controller.Configure(ValidParameters());

    EXPECT_EQ(controller.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.001f }, foc::RadiansPerSecond{ 0.0f } }).kind,
        foc::PositionOutputKind::currentReference);
}

TEST_F(TestLqrPositionController, the_command_follows_the_independently_designed_state_feedback)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.001f;

    controller.Configure(ValidParameters());

    const float expected = ExpectedCurrent(tunings, error, 0.0f);
    ASSERT_LT(std::abs(expected), maxCurrent);
    EXPECT_NEAR(Command(error, 0.0f), expected, tolerance);
}

TEST_F(TestLqrPositionController, the_speed_state_damps_the_position_command)
{
    const auto tunings = foc::PositionLoopTunings{};
    const float error = 0.001f;
    const float speed = 0.05f;

    controller.Configure(ValidParameters());

    const float expected = ExpectedCurrent(tunings, error, speed);
    ASSERT_LT(std::abs(expected), maxCurrent);
    EXPECT_NEAR(Command(error, 0.0f, speed), expected, tolerance);
    EXPECT_LT(Command(error, 0.0f, speed), Command(error, 0.0f, 0.0f));
}

TEST_F(TestLqrPositionController, a_matched_position_at_standstill_commands_no_current)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(0.5f, 0.5f), 0.0f, tolerance);
}

TEST_F(TestLqrPositionController, a_large_error_saturates_at_the_current_envelope)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(3.0f, 0.0f), maxCurrent, tolerance);
    EXPECT_NEAR(Command(-3.0f, 0.0f), -maxCurrent, tolerance);
}

TEST_F(TestLqrPositionController, an_error_across_the_seam_drives_the_short_way_forward)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(-pi + 0.0005f, pi - 0.0005f), Command(0.001f, 0.0f), tolerance);
}

TEST_F(TestLqrPositionController, an_error_across_the_seam_drives_the_short_way_backward)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(pi - 0.0005f, -pi + 0.0005f), Command(-0.001f, 0.0f), tolerance);
}

TEST_F(TestLqrPositionController, the_law_is_stateless_across_a_reset)
{
    controller.Configure(ValidParameters());
    const float before = Command(0.001f, 0.0f);

    controller.Reset();

    EXPECT_NEAR(Command(0.001f, 0.0f), before, tolerance);
}

TEST_F(TestLqrPositionController, the_closed_loop_brings_an_undisturbed_rigid_body_onto_the_reference)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(SettledPosition(0.5f), 0.5f, 1e-3f);
}

TEST_F(TestLqrPositionController, a_higher_bandwidth_asks_for_more_current_on_the_same_error)
{
    const float error = 0.0005f;

    controller.Configure(ValidParameters());
    const float slow = Command(error, 0.0f);

    auto tunings = foc::PositionLoopTunings{};
    tunings.bandwidth = 4.0f * tunings.bandwidth;
    controller.SetTunings(tunings);

    EXPECT_GT(Command(error, 0.0f), slow);
}
