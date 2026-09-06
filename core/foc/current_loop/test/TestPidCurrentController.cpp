#include "core/foc/current_loop/PidCurrentController.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <limits>
#include <numbers>

namespace
{
    constexpr float tolerance = 1e-4f;

    foc::MotorModelParameters ValidParameters()
    {
        return { foc::Ohm{ 1.0f }, foc::MilliHenry{ 0.5f }, foc::Weber{ 0.01f }, foc::Volts{ 24.0f }, hal::Hertz{ 20000 } };
    }

    class TestPidCurrentController
        : public ::testing::Test
    {
    public:
        foc::PidCurrentController controller;
    };
}

TEST_F(TestPidCurrentController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestPidCurrentController, positive_error_produces_positive_voltage)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_GT(output.d, 0.0f);
    EXPECT_GT(output.q, 0.0f);
}

TEST_F(TestPidCurrentController, negative_error_produces_negative_voltage)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 1.0f, 1.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_LT(output.d, 0.0f);
    EXPECT_LT(output.q, 0.0f);
}

TEST_F(TestPidCurrentController, saturated_output_stays_inside_the_modulation_circle)
{
    controller.Configure(ValidParameters());
    controller.SetTunings({ 1e6f, 1.0f, 0.2f, false });

    auto output = controller.Compute({ { -100.0f, -100.0f }, { 100.0f, 100.0f }, 0.0f });

    EXPECT_NEAR(std::hypot(output.d, output.q), 1.0f, tolerance);
}

TEST_F(TestPidCurrentController, reset_clears_accumulated_output)
{
    controller.Configure(ValidParameters());
    controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    controller.Reset();
    auto output = controller.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestPidCurrentController, invalid_parameters_leave_gains_untouched)
{
    controller.Configure({ foc::Ohm{ 0.0f }, foc::MilliHenry{ 0.0f }, foc::Weber{ 0.0f }, foc::Volts{ 0.0f }, hal::Hertz{ 0 } });

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestPidCurrentController, gains_follow_the_pole_zero_cancellation_design)
{
    const auto parameters = ValidParameters();
    const float bandwidth = 500.0f;
    const float error = 0.01f;

    controller.Configure(parameters);
    controller.SetTunings({ bandwidth, 1.0f, 0.2f, false });

    const float scale = std::numbers::sqrt3_v<float> / parameters.busVoltage.Value();
    const float samplePeriod = 1.0f / static_cast<float>(parameters.samplingFrequency.Value());
    const float kp = parameters.inductance.Value() * 0.001f * bandwidth * scale;
    const float ki = parameters.resistance.Value() * bandwidth * scale * samplePeriod;

    auto output = controller.Compute({ { 0.0f, 0.0f }, { error, error }, 0.0f });

    EXPECT_NEAR(output.d, (kp + ki) * error, tolerance);
    EXPECT_NEAR(output.q, (kp + ki) * error, tolerance);
}

TEST_F(TestPidCurrentController, integral_gain_scales_with_the_sample_period)
{
    const float bandwidth = 500.0f;
    const float error = 0.01f;

    auto fast = ValidParameters();
    auto slow = ValidParameters();
    slow.samplingFrequency = hal::Hertz{ fast.samplingFrequency.Value() / 2 };

    foc::PidCurrentController fastController;
    fastController.Configure(fast);
    fastController.SetTunings({ bandwidth, 1.0f, 0.2f, false });

    foc::PidCurrentController slowController;
    slowController.Configure(slow);
    slowController.SetTunings({ bandwidth, 1.0f, 0.2f, false });

    const float scale = std::numbers::sqrt3_v<float> / fast.busVoltage.Value();
    const float kp = fast.inductance.Value() * 0.001f * bandwidth * scale;
    const float kiFast = fast.resistance.Value() * bandwidth * scale / static_cast<float>(fast.samplingFrequency.Value());

    const auto fastOutput = fastController.Compute({ { 0.0f, 0.0f }, { error, error }, 0.0f });
    const auto slowOutput = slowController.Compute({ { 0.0f, 0.0f }, { error, error }, 0.0f });

    EXPECT_NEAR(fastOutput.d, (kp + kiFast) * error, tolerance);
    EXPECT_NEAR(slowOutput.d, (kp + 2.0f * kiFast) * error, tolerance);
}

TEST_F(TestPidCurrentController, proportional_gain_tracks_inductance_and_integral_gain_tracks_resistance)
{
    auto parameters = ValidParameters();
    parameters.inductance = foc::MilliHenry{ parameters.inductance.Value() * 2.0f };
    parameters.resistance = foc::Ohm{ parameters.resistance.Value() * 3.0f };

    const float bandwidth = 500.0f;
    const float error = 0.005f;

    controller.Configure(parameters);
    controller.SetTunings({ bandwidth, 1.0f, 0.2f, false });

    const float scale = std::numbers::sqrt3_v<float> / parameters.busVoltage.Value();
    const float kp = parameters.inductance.Value() * 0.001f * bandwidth * scale;
    const float ki = parameters.resistance.Value() * bandwidth * scale / static_cast<float>(parameters.samplingFrequency.Value());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { error, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, (kp + ki) * error, tolerance);
}

// Anti-windup: the incremental form clamps its own accumulator, so a long saturating excursion must
// not leave stored integral that delays the return once the error reverses.
TEST_F(TestPidCurrentController, sustained_saturation_does_not_wind_up_the_integrator)
{
    controller.Configure(ValidParameters());

    for (int i = 0; i < 200; ++i)
        controller.Compute({ { 0.0f, 0.0f }, { 50.0f, 0.0f }, 0.0f });

    const auto saturated = controller.Compute({ { 0.0f, 0.0f }, { 50.0f, 0.0f }, 0.0f });
    EXPECT_NEAR(saturated.d, 1.0f, tolerance);

    const auto reversed = controller.Compute({ { 0.0f, 0.0f }, { -50.0f, 0.0f }, 0.0f });
    EXPECT_NEAR(reversed.d, -1.0f, tolerance);
}

TEST_F(TestPidCurrentController, saturated_output_stays_on_the_modulation_circle)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 200; ++sample)
        controller.Compute({ { 0.0f, 0.0f }, { 100.0f, 100.0f }, 0.0f });

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 100.0f, 100.0f }, 0.0f });

    EXPECT_NEAR(std::sqrt(output.d * output.d + output.q * output.q), 1.0f, tolerance);
}

TEST_F(TestPidCurrentController, a_reference_reversal_after_saturation_recovers_within_one_sample)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 200; ++sample)
        controller.Compute({ { 0.0f, 0.0f }, { 100.0f, 100.0f }, 0.0f });

    auto output = controller.Compute({ { 0.0f, 0.0f }, { -100.0f, -100.0f }, 0.0f });

    EXPECT_LT(output.d, 0.0f);
    EXPECT_LT(output.q, 0.0f);
}

TEST_F(TestPidCurrentController, an_unreachable_reference_does_not_wind_the_integrator_past_the_circle)
{
    controller.Configure(ValidParameters());

    for (int sample = 0; sample != 5000; ++sample)
        controller.Compute({ { 0.0f, 0.0f }, { 1000.0f, 1000.0f }, 0.0f });

    auto recovered = controller.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_LE(std::sqrt(recovered.d * recovered.d + recovered.q * recovered.q), 1.0f + tolerance);
}

TEST_F(TestPidCurrentController, a_well_conditioned_plant_is_usable)
{
    const auto plant = foc::CurrentPlantModel::FromParameters(ValidParameters());

    EXPECT_TRUE(plant.IsUsable());
}

TEST_F(TestPidCurrentController, an_inductance_far_above_the_sample_period_is_rejected)
{
    auto parameters = ValidParameters();
    parameters.inductance = foc::MilliHenry{ 1.0e9f };

    EXPECT_FALSE(foc::CurrentPlantModel::FromParameters(parameters).IsUsable());
}

TEST_F(TestPidCurrentController, a_default_constructed_plant_is_rejected)
{
    EXPECT_FALSE(foc::CurrentPlantModel{}.IsUsable());
}

TEST_F(TestPidCurrentController, a_plant_with_a_non_finite_gain_is_rejected)
{
    EXPECT_FALSE((foc::CurrentPlantModel{ std::numeric_limits<float>::quiet_NaN(), 0.5f }.IsUsable()));
    EXPECT_FALSE((foc::CurrentPlantModel{ 0.5f, std::numeric_limits<float>::infinity() }.IsUsable()));
}

TEST_F(TestPidCurrentController, a_plant_whose_state_gain_leaves_the_unit_interval_is_rejected)
{
    EXPECT_FALSE((foc::CurrentPlantModel{ -0.1f, 0.5f }.IsUsable()));
    EXPECT_FALSE((foc::CurrentPlantModel{ 1.0f, 0.5f }.IsUsable()));
}
