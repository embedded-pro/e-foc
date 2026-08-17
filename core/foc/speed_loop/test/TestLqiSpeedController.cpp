#include "core/foc/speed_loop/LqiSpeedController.hpp"
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

    class TestLqiSpeedController
        : public ::testing::Test
    {
    public:
        float RunClosedLoop(float reference, std::size_t steps)
        {
            const auto plant = foc::SpeedPlantModel::FromParameters(ValidParameters());
            float speed{ 0.0f };

            for (std::size_t step = 0; step != steps; ++step)
            {
                auto current = controller.Compute({ foc::RadiansPerSecond{ speed }, foc::RadiansPerSecond{ reference } });
                speed = plant.ad * speed + plant.bd * current.Value();
            }

            return speed;
        }

        float RelativeOvershoot(float reference, std::size_t steps)
        {
            const auto plant = foc::SpeedPlantModel::FromParameters(ValidParameters());
            float speed{ 0.0f };
            float peak{ 0.0f };

            for (std::size_t step = 0; step != steps; ++step)
            {
                auto current = controller.Compute({ foc::RadiansPerSecond{ speed }, foc::RadiansPerSecond{ reference } });
                speed = plant.ad * speed + plant.bd * current.Value();
                peak = std::max(peak, speed);
            }

            return peak / reference - 1.0f;
        }

        foc::LqiSpeedController controller;
    };
}

TEST_F(TestLqiSpeedController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestLqiSpeedController, positive_error_produces_positive_current)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_GT(output.Value(), 0.0f);
}

TEST_F(TestLqiSpeedController, integral_action_removes_steady_state_error)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(RunClosedLoop(50.0f, 20000), 50.0f, 0.5f);
}

TEST_F(TestLqiSpeedController, output_stays_inside_the_current_envelope)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0e6f } });

    EXPECT_NEAR(output.Value(), ValidParameters().maxCurrent.Value(), tolerance);
}

TEST_F(TestLqiSpeedController, saturation_freezes_the_integrator)
{
    controller.Configure(ValidParameters());

    const auto plant = foc::SpeedPlantModel::FromParameters(ValidParameters());
    float speed{ 0.0f };
    float peak{ 0.0f };

    // The step is far enough away that the current stays clipped for hundreds of samples
    for (std::size_t step = 0; step != 5000; ++step)
    {
        auto current = controller.Compute({ foc::RadiansPerSecond{ speed }, foc::RadiansPerSecond{ 2000.0f } });
        speed = plant.ad * speed + plant.bd * current.Value();
        peak = std::max(peak, speed);
    }

    EXPECT_LT(peak, 2200.0f);
    EXPECT_NEAR(speed, 2000.0f, 1.0f);
}

TEST_F(TestLqiSpeedController, a_saturated_sample_leaves_the_integral_state_untouched)
{
    controller.Configure(ValidParameters());

    const foc::SpeedControlContext saturating{ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0e4f } };
    auto first = controller.Compute(saturating);

    for (std::size_t step = 0; step != 50; ++step)
        EXPECT_NEAR(controller.Compute(saturating).Value(), first.Value(), tolerance);

    auto released = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(released.Value(), 0.0f, tolerance);
}

TEST_F(TestLqiSpeedController, a_saturating_step_overshoots_no_more_than_a_linear_one)
{
    controller.Configure(ValidParameters());
    const auto saturated = RelativeOvershoot(2000.0f, 5000);

    controller.Reset();
    const auto linear = RelativeOvershoot(1.0f, 5000);

    EXPECT_LE(saturated, linear + 1.0e-3f);
}

TEST_F(TestLqiSpeedController, reset_clears_the_integrator)
{
    controller.Configure(ValidParameters());
    RunClosedLoop(50.0f, 1000);

    controller.Reset();
    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestLqiSpeedController, invalid_parameters_leave_the_controller_inert)
{
    controller.Configure({ foc::NewtonMeterSecondSquared{ 0.0f }, foc::NewtonMeterSecondPerRadian{ 0.0f },
        foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, hal::Hertz{ 0 } });

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}
