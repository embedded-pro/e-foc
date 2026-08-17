#include "core/foc/speed_loop/AdrcSpeedController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include <algorithm>
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    class TestAdrcSpeedController
        : public ::testing::Test
    {
    public:
        float RunClosedLoop(float reference, float loadDisturbance, std::size_t steps)
        {
            const auto plant = foc::SpeedPlantModel::FromParameters(ValidParameters());
            float speed{ 0.0f };

            for (std::size_t step = 0; step != steps; ++step)
            {
                auto current = controller.Compute({ foc::RadiansPerSecond{ speed }, foc::RadiansPerSecond{ reference } });
                speed = plant.ad * speed + plant.bd * current.Value() + loadDisturbance;
            }

            return speed;
        }

        foc::AdrcSpeedController controller;
    };
}

TEST_F(TestAdrcSpeedController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestAdrcSpeedController, positive_error_produces_positive_current)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_GT(output.Value(), 0.0f);
}

TEST_F(TestAdrcSpeedController, tracks_a_speed_reference)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(RunClosedLoop(20.0f, 0.0f, 2000), 20.0f, 0.5f);
}

TEST_F(TestAdrcSpeedController, observer_cancels_a_constant_load_disturbance)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(RunClosedLoop(20.0f, 0.01f, 2000), 20.0f, 0.5f);
}

TEST_F(TestAdrcSpeedController, output_stays_inside_the_current_envelope)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0e6f } });

    EXPECT_NEAR(output.Value(), ValidParameters().maxCurrent.Value(), tolerance);
}

TEST_F(TestAdrcSpeedController, saturating_step_does_not_wind_up_the_disturbance_estimate)
{
    controller.Configure(ValidParameters());

    const auto plant = foc::SpeedPlantModel::FromParameters(ValidParameters());
    float speed{ 0.0f };
    float peak{ 0.0f };

    // The step demands roughly 38 A against a 10 A envelope, so the command is clipped for hundreds of samples
    for (std::size_t step = 0; step != 8000; ++step)
    {
        auto current = controller.Compute({ foc::RadiansPerSecond{ speed }, foc::RadiansPerSecond{ 100.0f } });
        speed = plant.ad * speed + plant.bd * current.Value();
        peak = std::max(peak, speed);
    }

    EXPECT_LT(peak, 102.0f);
    EXPECT_NEAR(speed, 100.0f, 0.5f);
}

TEST_F(TestAdrcSpeedController, reset_clears_the_observer_state)
{
    controller.Configure(ValidParameters());
    RunClosedLoop(20.0f, 0.01f, 1000);

    controller.Reset();
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestAdrcSpeedController, invalid_parameters_leave_the_controller_inert)
{
    controller.Configure({ foc::NewtonMeterSecondSquared{ 0.0f }, foc::NewtonMeterSecondPerRadian{ 0.0f },
        foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, hal::Hertz{ 0 } });

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}
