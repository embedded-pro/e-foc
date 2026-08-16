#include "core/services/speed_controllers/TwoDofSpeedController.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    services::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    services::SpeedControllerTunings TuningsWithReferenceTimeConstant(float referenceTimeConstant)
    {
        auto tunings = services::SpeedControllerTunings{};
        tunings.referenceTimeConstant = referenceTimeConstant;

        return tunings;
    }

    class TestTwoDofSpeedController
        : public ::testing::Test
    {
    public:
        services::TwoDofSpeedController controller;
    };
}

TEST_F(TestTwoDofSpeedController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestTwoDofSpeedController, positive_error_produces_positive_current)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_GT(output.Value(), 0.0f);
}

TEST_F(TestTwoDofSpeedController, pre_filter_softens_a_setpoint_step)
{
    controller.Configure(ValidParameters());

    services::PidSpeedController withoutPreFilter;
    withoutPreFilter.Configure(ValidParameters());

    const services::SpeedControlContext step{ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } };

    EXPECT_LT(controller.Compute(step).Value(), withoutPreFilter.Compute(step).Value());
}

TEST_F(TestTwoDofSpeedController, a_longer_time_constant_slows_the_reference_further)
{
    controller.Configure(ValidParameters());
    controller.SetTunings(TuningsWithReferenceTimeConstant(0.1f));

    services::TwoDofSpeedController responsive;
    responsive.Configure(ValidParameters());
    responsive.SetTunings(TuningsWithReferenceTimeConstant(0.001f));

    const services::SpeedControlContext step{ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } };

    EXPECT_LT(controller.Compute(step).Value(), responsive.Compute(step).Value());
}

TEST_F(TestTwoDofSpeedController, a_non_positive_time_constant_passes_the_reference_through)
{
    controller.Configure(ValidParameters());
    controller.SetTunings(TuningsWithReferenceTimeConstant(0.0f));

    services::PidSpeedController withoutPreFilter;
    withoutPreFilter.Configure(ValidParameters());

    const services::SpeedControlContext step{ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } };

    EXPECT_NEAR(controller.Compute(step).Value(), withoutPreFilter.Compute(step).Value(), tolerance);
}

TEST_F(TestTwoDofSpeedController, reset_clears_the_pre_filter_and_the_feedback)
{
    controller.Configure(ValidParameters());
    controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    controller.Reset();
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestTwoDofSpeedController, invalid_parameters_leave_the_controller_inert)
{
    controller.Configure({ foc::NewtonMeterSecondSquared{ 0.0f }, foc::NewtonMeterSecondPerRadian{ 0.0f },
        foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, hal::Hertz{ 0 } });

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}
