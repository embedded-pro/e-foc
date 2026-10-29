#include "core/foc/current_loop/DeadbeatCurrentController.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1e-4f;
    constexpr float resistance = 1.0f;
    constexpr float inductanceInHenry = 0.0005f;
    constexpr float samplePeriod = 1.0f / 20000.0f;
    constexpr float busVoltage = 24.0f;

    foc::MotorModelParameters ValidParameters()
    {
        return { foc::Ohm{ resistance }, foc::MilliHenry{ 0.5f }, foc::Weber{ 0.01f }, foc::Volts{ busVoltage }, hal::Hertz{ 20000 } };
    }

    float Ad()
    {
        return std::exp(-resistance * samplePeriod / inductanceInHenry);
    }

    float Bd()
    {
        return (1.0f - Ad()) / resistance;
    }

    float Normalized(float physicalVoltage)
    {
        return physicalVoltage * std::numbers::sqrt3_v<float> / busVoltage;
    }

    foc::MotorModelParameters ParametersWithALateDuty()
    {
        auto parameters = ValidParameters();
        parameters.dutyUpdateDelayPeriods = 1;
        return parameters;
    }

    class TestDeadbeatCurrentController
        : public ::testing::Test
    {
    public:
        float SettledCurrent(float reference, float disturbanceVoltage = 0.0f, bool lateDuty = false, int steps = 500)
        {
            const float denormalize = busVoltage / std::numbers::sqrt3_v<float>;
            float current = 0.0f;
            float pendingVoltage = 0.0f;

            for (int step = 0; step < steps; ++step)
            {
                auto output = controller.Compute({ { current, 0.0f }, { reference, 0.0f }, 0.0f });
                const float voltage = output.d * denormalize;
                current = Ad() * current + Bd() * ((lateDuty ? pendingVoltage : voltage) + disturbanceVoltage);
                pendingVoltage = voltage;
            }

            return current;
        }

        foc::DeadbeatCurrentController controller;
    };
}

TEST_F(TestDeadbeatCurrentController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestDeadbeatCurrentController, one_step_law_inverts_the_discrete_plant)
{
    controller.Configure(ValidParameters());

    const float reference = 0.1f;
    const float measured = 0.05f;
    auto output = controller.Compute({ { measured, measured }, { reference, reference }, 0.0f });

    const float expected = Normalized((reference - Ad() * measured) / Bd());
    EXPECT_NEAR(output.d, expected, tolerance);
    EXPECT_NEAR(output.q, expected, tolerance);
}

TEST_F(TestDeadbeatCurrentController, two_step_law_trades_state_gain_for_a_unity_dc_gain_reference_term)
{
    controller.Configure(ValidParameters());
    controller.SetTunings({ 6283.185307f, 1.0f, 0.2f, true });

    const float reference = 0.1f;
    const float measured = 0.05f;
    auto output = controller.Compute({ { measured, measured }, { reference, reference }, 0.0f });

    const float ad = Ad();
    const float expected = Normalized(((ad * ad - ad + 1.0f) * reference - ad * ad * ad * measured) / (Bd() * (ad * ad + 1.0f)));
    EXPECT_NEAR(output.d, expected, tolerance);
    EXPECT_NEAR(output.q, expected, tolerance);
}

TEST_F(TestDeadbeatCurrentController, two_step_law_amplifies_measurement_noise_less_than_one_step)
{
    const foc::CurrentControlContext quiet{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f };
    const foc::CurrentControlContext noisy{ { 0.01f, 0.0f }, { 0.0f, 0.0f }, 0.0f };

    controller.Configure(ValidParameters());
    const float oneStepNoisy = controller.Compute(noisy).d;
    controller.Reset();
    const float oneStepResponse = oneStepNoisy - controller.Compute(quiet).d;

    controller.SetTunings({ 6283.185307f, 1.0f, 0.2f, true });
    controller.Reset();
    const float twoStepNoisy = controller.Compute(noisy).d;
    controller.Reset();
    const float twoStepResponse = twoStepNoisy - controller.Compute(quiet).d;

    EXPECT_LT(std::abs(twoStepResponse), std::abs(oneStepResponse));
}

TEST_F(TestDeadbeatCurrentController, zero_error_at_steady_state_still_supplies_resistive_drop)
{
    controller.Configure(ValidParameters());

    const float current = 1.0f;
    auto output = controller.Compute({ { current, current }, { current, current }, 0.0f });

    const float expected = Normalized((1.0f - Ad()) * current / Bd());
    EXPECT_NEAR(output.d, expected, tolerance);
}

TEST_F(TestDeadbeatCurrentController, saturated_output_stays_inside_the_modulation_circle)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 100.0f, -100.0f }, 0.0f });

    const float diagonal = 1.0f / std::numbers::sqrt2_v<float>;
    EXPECT_NEAR(std::hypot(output.d, output.q), 1.0f, tolerance);
    EXPECT_NEAR(output.d, diagonal, tolerance);
    EXPECT_NEAR(output.q, -diagonal, tolerance);
}

TEST_F(TestDeadbeatCurrentController, one_step_law_tracks_the_reference_exactly_in_closed_loop)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(SettledCurrent(1.0f), 1.0f, tolerance);
}

TEST_F(TestDeadbeatCurrentController, two_step_law_tracks_the_reference_exactly_in_closed_loop)
{
    controller.Configure(ValidParameters());
    controller.SetTunings({ 6283.185307f, 1.0f, 0.2f, true });

    EXPECT_NEAR(SettledCurrent(1.0f), 1.0f, tolerance);
}

TEST_F(TestDeadbeatCurrentController, reset_does_not_change_the_stateless_output)
{
    controller.Configure(ValidParameters());
    auto before = controller.Compute({ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f });

    controller.Reset();
    auto after = controller.Compute({ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f });

    EXPECT_NEAR(before.d, after.d, tolerance);
    EXPECT_NEAR(before.q, after.q, tolerance);
}

TEST_F(TestDeadbeatCurrentController, with_a_late_duty_the_law_acts_on_the_current_it_predicts_for_when_the_duty_lands)
{
    controller.Configure(ParametersWithALateDuty());

    const float reference = 0.1f;
    const float measured = 0.05f;
    auto output = controller.Compute({ { measured, measured }, { reference, reference }, 0.0f });

    const float expected = Normalized((reference - Ad() * Ad() * measured) / Bd());
    EXPECT_NEAR(output.d, expected, tolerance);
    EXPECT_NEAR(output.q, expected, tolerance);
}

TEST_F(TestDeadbeatCurrentController, with_a_late_duty_the_one_step_law_reaches_the_reference_one_period_later)
{
    controller.Configure(ParametersWithALateDuty());

    const float denormalize = busVoltage / std::numbers::sqrt3_v<float>;
    const float reference = 1.0f;
    float current = 0.0f;
    float pendingVoltage = 0.0f;

    for (int step = 0; step < 2; ++step)
    {
        auto output = controller.Compute({ { current, 0.0f }, { reference, 0.0f }, 0.0f });
        current = Ad() * current + Bd() * pendingVoltage;
        pendingVoltage = output.d * denormalize;
    }

    EXPECT_NEAR(Ad() * current + Bd() * pendingVoltage, reference, tolerance);
}

TEST_F(TestDeadbeatCurrentController, reset_forgets_the_duty_still_in_flight)
{
    controller.Configure(ParametersWithALateDuty());
    const foc::CurrentControlContext context{ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f };
    auto first = controller.Compute(context);
    controller.Compute(context);

    controller.Reset();
    auto afterReset = controller.Compute(context);

    EXPECT_NEAR(first.d, afterReset.d, tolerance);
    EXPECT_NEAR(first.q, afterReset.q, tolerance);
}

TEST_F(TestDeadbeatCurrentController, a_constant_voltage_error_leaves_no_steady_state_current_error)
{
    controller.Configure(ValidParameters());

    EXPECT_NEAR(SettledCurrent(1.0f, 2.0f), 1.0f, tolerance);
}

TEST_F(TestDeadbeatCurrentController, with_a_late_duty_a_constant_voltage_error_leaves_no_steady_state_current_error)
{
    controller.Configure(ParametersWithALateDuty());

    EXPECT_NEAR(SettledCurrent(1.0f, 2.0f, true), 1.0f, tolerance);
}

TEST_F(TestDeadbeatCurrentController, a_wider_bandwidth_rejects_a_voltage_error_sooner)
{
    controller.Configure(ParametersWithALateDuty());
    controller.SetTunings({ 1000.0f, 1.0f, 0.2f, false });
    const float narrowError = std::abs(SettledCurrent(0.0f, 2.0f, true, 20));

    controller.SetTunings({ 6283.185307f, 1.0f, 0.2f, false });
    controller.Reset();
    const float wideError = std::abs(SettledCurrent(0.0f, 2.0f, true, 20));

    EXPECT_LT(wideError, narrowError);
}

TEST_F(TestDeadbeatCurrentController, reset_forgets_the_voltage_error_it_learned)
{
    controller.Configure(ParametersWithALateDuty());
    const foc::CurrentControlContext context{ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f };
    auto first = controller.Compute(context);
    SettledCurrent(0.0f, 2.0f, true);

    controller.Reset();
    auto afterReset = controller.Compute(context);

    EXPECT_NEAR(first.d, afterReset.d, tolerance);
    EXPECT_NEAR(first.q, afterReset.q, tolerance);
}
