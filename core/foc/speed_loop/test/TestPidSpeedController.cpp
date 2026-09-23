#include "core/foc/speed_loop/PidSpeedController.hpp"
#include <algorithm>
#include <cmath>
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    foc::MechanicalModelParameters ReferenceMotorParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 7.06e-6f }, foc::NewtonMeterSecondPerRadian{ 1.5e-5f },
            foc::NewtonMeter{ 0.0384f }, foc::Ampere{ 20.0f }, hal::Hertz{ 1000 } };
    }

    struct TorqueStepResponse
    {
        float settledError{ 0.0f };
        float lastExcursionSeconds{ 0.0f };
    };

    class TestPidSpeedController
        : public ::testing::Test
    {
    public:
        TorqueStepResponse RejectTorqueStep(const foc::MechanicalModelParameters& parameters, float reference, float torque, float band)
        {
            constexpr std::size_t substeps{ 20 };
            constexpr float samplePeriod{ 1.0e-3f };
            constexpr std::size_t stepSample{ 300 };
            constexpr std::size_t samples{ 750 };
            const auto dt = samplePeriod / static_cast<float>(substeps);
            float speed{ 0.0f };
            float pendingCurrent{ 0.0f };
            TorqueStepResponse response;

            controller.Configure(parameters);
            controller.SetTunings({ 100.0f, 1.0f, 0.1f, 5.0f, 0.005f });

            for (std::size_t sample = 0; sample != samples; ++sample)
            {
                const auto current = pendingCurrent;
                pendingCurrent = controller.Compute({ foc::RadiansPerSecond{ speed }, foc::RadiansPerSecond{ reference } }).Value();
                const auto load = sample >= stepSample ? torque : 0.0f;

                for (std::size_t substep = 0; substep != substeps; ++substep)
                    speed += (parameters.torqueConstant.Value() * current - parameters.viscousFriction.Value() * speed - load) / parameters.inertia.Value() * dt;

                if (sample >= stepSample && std::abs(speed - reference) > band)
                    response.lastExcursionSeconds = static_cast<float>(sample - stepSample) * samplePeriod;
            }

            response.settledError = reference - speed;
            return response;
        }

        foc::PidSpeedController controller;
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

    foc::PidSpeedController fast;
    fast.Configure(ValidParameters());
    fast.SetTunings({ 2.0f * foc::SpeedLoopTunings{}.bandwidth, 1.0f, 0.1f, 5.0f, 0.005f });

    EXPECT_GT(fast.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } }).Value(), slow.Value());
}

TEST_F(TestPidSpeedController, invalid_parameters_leave_gains_untouched)
{
    controller.Configure({ foc::NewtonMeterSecondSquared{ 0.0f }, foc::NewtonMeterSecondPerRadian{ 0.0f },
        foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, hal::Hertz{ 0 } });

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestPidSpeedController, gains_follow_the_inertia_based_design)
{
    const auto parameters = ValidParameters();
    const float bandwidth = 50.0f;
    const float error = 1.0f;

    controller.Configure(parameters);
    controller.SetTunings({ bandwidth, 1.0f, 0.1f, 5.0f, 0.005f });

    const float inertia = parameters.inertia.Value();
    const float samplePeriod = 1.0f / static_cast<float>(parameters.samplingFrequency.Value());
    const float kp = 2.0f * inertia * bandwidth / parameters.torqueConstant.Value();
    const float integralZero = std::max(parameters.viscousFriction.Value() / inertia, bandwidth / 4.0f);
    const float ki = kp * integralZero * samplePeriod;

    auto output = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ error } });

    EXPECT_NEAR(output.Value(), (kp + ki) * error, tolerance);
}

TEST_F(TestPidSpeedController, sustained_saturation_does_not_wind_up_the_integrator)
{
    controller.Configure(ValidParameters());

    for (int i = 0; i < 200; ++i)
        controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1000.0f } });

    const auto saturated = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1000.0f } });
    EXPECT_NEAR(saturated.Value(), ValidParameters().maxCurrent.Value(), tolerance);

    const auto reversed = controller.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ -1000.0f } });
    EXPECT_NEAR(reversed.Value(), -ValidParameters().maxCurrent.Value(), tolerance);
}

TEST_F(TestPidSpeedController, a_near_saturating_torque_step_is_rejected_within_the_window)
{
    const auto response = RejectTorqueStep(ReferenceMotorParameters(), 20.0f, 0.5f, 3.0f);

    EXPECT_LT(response.lastExcursionSeconds, 0.35f);
    EXPECT_LT(std::abs(response.settledError), 0.5f);
}
