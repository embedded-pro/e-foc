#include "core/foc/position_loop/TwoDofPositionController.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1e-4f;
    constexpr float nearSeam = 3.0f;

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    foc::PositionLoopTunings TuningsWithReferenceTimeConstant(float referenceTimeConstant)
    {
        auto tunings = foc::PositionLoopTunings{};
        tunings.referenceTimeConstant = referenceTimeConstant;

        return tunings;
    }

    class TestTwoDofPositionController
        : public ::testing::Test
    {
    public:
        float FirstCommand(float measured, float reference)
        {
            return controller.Compute({ foc::Radians{ measured }, foc::Radians{ reference } }).value;
        }

        foc::TwoDofPositionController controller;
    };
}

TEST_F(TestTwoDofPositionController, unconfigured_controller_outputs_zero)
{
    EXPECT_NEAR(FirstCommand(0.0f, 1.0f), 0.0f, tolerance);
}

TEST_F(TestTwoDofPositionController, positive_error_produces_a_positive_speed_reference)
{
    controller.Configure(ValidParameters());

    EXPECT_GT(FirstCommand(0.0f, 1.0f), 0.0f);
}

TEST_F(TestTwoDofPositionController, a_setpoint_across_the_seam_is_approached_the_short_way_forward)
{
    controller.Configure(ValidParameters());

    EXPECT_GT(FirstCommand(nearSeam, -nearSeam), 0.0f);
}

TEST_F(TestTwoDofPositionController, a_setpoint_across_the_seam_is_approached_the_short_way_backward)
{
    controller.Configure(ValidParameters());

    EXPECT_LT(FirstCommand(-nearSeam, nearSeam), 0.0f);
}

TEST_F(TestTwoDofPositionController, the_shaped_reference_never_leaves_the_seam_neighbourhood)
{
    controller.Configure(ValidParameters());

    const float shortestPath = 2.0f * std::numbers::pi_v<float> - 2.0f * nearSeam;
    const float speedEnvelope = foc::PositionLoopTunings{}.bandwidth * std::numbers::pi_v<float>;

    EXPECT_LT(FirstCommand(nearSeam, -nearSeam), shortestPath / std::numbers::pi_v<float> * speedEnvelope);
}

TEST_F(TestTwoDofPositionController, the_first_sample_after_reset_does_not_command_motion_towards_zero)
{
    controller.Configure(ValidParameters());
    controller.Compute({ foc::Radians{ 0.0f }, foc::Radians{ 0.0f } });

    controller.Reset();

    EXPECT_NEAR(FirstCommand(nearSeam, nearSeam), 0.0f, tolerance);
}

TEST_F(TestTwoDofPositionController, a_non_positive_time_constant_passes_the_reference_through)
{
    controller.Configure(ValidParameters());
    controller.SetTunings(TuningsWithReferenceTimeConstant(0.0f));

    foc::PidPositionController withoutPreFilter;
    withoutPreFilter.Configure(ValidParameters());
    withoutPreFilter.SetTunings(TuningsWithReferenceTimeConstant(0.0f));

    const foc::PositionControlContext step{ foc::Radians{ 0.0f }, foc::Radians{ 1.0f } };

    EXPECT_NEAR(controller.Compute(step).value, withoutPreFilter.Compute(step).value, tolerance);
}

TEST_F(TestTwoDofPositionController, the_pre_filter_softens_a_setpoint_step)
{
    controller.Configure(ValidParameters());

    foc::PidPositionController withoutPreFilter;
    withoutPreFilter.Configure(ValidParameters());

    const foc::PositionControlContext step{ foc::Radians{ 0.0f }, foc::Radians{ 1.0f } };

    EXPECT_LT(controller.Compute(step).value, withoutPreFilter.Compute(step).value);
}

TEST_F(TestTwoDofPositionController, a_longer_time_constant_slows_the_reference_further)
{
    controller.Configure(ValidParameters());
    controller.SetTunings(TuningsWithReferenceTimeConstant(0.5f));

    foc::TwoDofPositionController responsive;
    responsive.Configure(ValidParameters());
    responsive.SetTunings(TuningsWithReferenceTimeConstant(0.005f));

    const foc::PositionControlContext step{ foc::Radians{ 0.0f }, foc::Radians{ 1.0f } };

    EXPECT_LT(controller.Compute(step).value, responsive.Compute(step).value);
}
