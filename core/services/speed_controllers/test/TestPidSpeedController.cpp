#include "core/services/speed_controllers/PidSpeedController.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    services::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    class TestPidSpeedController
        : public ::testing::Test
    {
    public:
        services::PidSpeedController controller;
    };
}

TEST_F(TestPidSpeedController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestPidSpeedController, positive_error_produces_positive_current)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_GT(output.Value(), 0.0f);
}

TEST_F(TestPidSpeedController, negative_error_produces_negative_current)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 10.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_LT(output.Value(), 0.0f);
}

TEST_F(TestPidSpeedController, saturated_output_stays_inside_the_current_envelope)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ -10000.0f }, foc::RadiansPerSecond{ 10000.0f } });

    EXPECT_NEAR(output.Value(), ValidParameters().maxCurrent.Value(), tolerance);
}

TEST_F(TestPidSpeedController, reset_clears_accumulated_output)
{
    controller.Configure(ValidParameters());
    controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    controller.Reset();
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestPidSpeedController, higher_bandwidth_produces_a_stronger_response)
{
    controller.Configure(ValidParameters());
    auto slow = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } });

    services::PidSpeedController fast;
    fast.Configure(ValidParameters());
    fast.SetTunings({ 2.0f * services::SpeedControllerTunings{}.bandwidth, 1.0f, 0.1f, 5.0f, 0.005f });

    EXPECT_GT(fast.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } }).Value(), slow.Value());
}

TEST_F(TestPidSpeedController, invalid_parameters_leave_gains_untouched)
{
    controller.Configure({ foc::NewtonMeterSecondSquared{ 0.0f }, foc::NewtonMeterSecondPerRadian{ 0.0f },
        foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, hal::Hertz{ 0 } });

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}
