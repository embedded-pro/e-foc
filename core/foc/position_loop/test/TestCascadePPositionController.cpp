#include "core/foc/position_loop/CascadePPositionController.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float tolerance = 1e-4f;

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    foc::PositionLoopTunings TuningsWithBandwidth(float bandwidth)
    {
        auto tunings = foc::PositionLoopTunings{};
        tunings.bandwidth = bandwidth;

        return tunings;
    }

    class TestCascadePPositionController
        : public ::testing::Test
    {
    public:
        float Command(float reference, float measured)
        {
            return controller.Compute({ foc::Radians{ measured }, foc::Radians{ reference }, foc::RadiansPerSecond{ 0.0f } }).value;
        }

        foc::CascadePPositionController controller;
    };
}

TEST_F(TestCascadePPositionController, the_command_is_a_speed_reference)
{
    EXPECT_EQ(controller.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.1f }, foc::RadiansPerSecond{ 0.0f } }).kind,
        foc::PositionOutputKind::speedReference);
}

TEST_F(TestCascadePPositionController, an_unconfigured_controller_runs_on_the_default_bandwidth)
{
    EXPECT_NEAR(Command(0.1f, 0.0f), 0.1f * foc::PositionLoopTunings{}.bandwidth, tolerance);
}

TEST_F(TestCascadePPositionController, the_proportional_gain_is_the_requested_bandwidth)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));

    EXPECT_NEAR(Command(0.1f, 0.0f), 0.5f, tolerance);
    EXPECT_NEAR(Command(-0.4f, 0.0f), -2.0f, tolerance);
}

TEST_F(TestCascadePPositionController, a_zero_bandwidth_holds_the_speed_reference_at_zero)
{
    controller.SetTunings(TuningsWithBandwidth(0.0f));

    EXPECT_NEAR(Command(1.0f, 0.0f), 0.0f, tolerance);
}

TEST_F(TestCascadePPositionController, the_mechanical_model_does_not_change_the_gain)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));
    controller.Configure(ValidParameters());

    EXPECT_NEAR(Command(0.1f, 0.0f), 0.5f, tolerance);
}

TEST_F(TestCascadePPositionController, a_matched_position_commands_no_motion)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));

    EXPECT_NEAR(Command(2.0f, 2.0f), 0.0f, tolerance);
}

TEST_F(TestCascadePPositionController, an_error_across_the_seam_commands_the_short_way_forward)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));

    EXPECT_NEAR(Command(-pi + 0.1f, pi - 0.1f), 1.0f, 1e-3f);
}

TEST_F(TestCascadePPositionController, an_error_across_the_seam_commands_the_short_way_backward)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));

    EXPECT_NEAR(Command(pi - 0.1f, -pi + 0.1f), -1.0f, 1e-3f);
}

TEST_F(TestCascadePPositionController, half_a_turn_of_error_sits_on_the_boundary_and_drives_forward)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));

    EXPECT_NEAR(Command(pi, 0.0f), 5.0f * pi, 1e-3f);
}

TEST_F(TestCascadePPositionController, the_law_is_stateless_across_a_reset)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));
    const float before = Command(0.1f, 0.0f);

    controller.Reset();

    EXPECT_NEAR(Command(0.1f, 0.0f), before, tolerance);
}

TEST_F(TestCascadePPositionController, the_law_carries_no_memory_of_the_previous_sample)
{
    controller.SetTunings(TuningsWithBandwidth(5.0f));
    Command(3.0f, 0.0f);

    EXPECT_NEAR(Command(0.1f, 0.0f), 0.5f, tolerance);
}
