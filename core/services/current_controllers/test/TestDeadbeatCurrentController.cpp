#include "core/services/current_controllers/DeadbeatCurrentController.hpp"
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

    services::MotorModelParameters ValidParameters()
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

    class TestDeadbeatCurrentController
        : public ::testing::Test
    {
    public:
        float SettledCurrent(float reference)
        {
            const float denormalize = busVoltage / std::numbers::sqrt3_v<float>;
            float current = 0.0f;

            for (int step = 0; step < 500; ++step)
            {
                auto output = controller.Compute({ { current, 0.0f }, { reference, 0.0f }, 0.0f });
                current = Ad() * current + Bd() * output.d * denormalize;
            }

            return current;
        }

        services::DeadbeatCurrentController controller;
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
    const services::CurrentControlContext quiet{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f };
    const services::CurrentControlContext noisy{ { 0.01f, 0.0f }, { 0.0f, 0.0f }, 0.0f };

    controller.Configure(ValidParameters());
    const float oneStepResponse = controller.Compute(noisy).d - controller.Compute(quiet).d;

    controller.SetTunings({ 6283.185307f, 1.0f, 0.2f, true });
    const float twoStepResponse = controller.Compute(noisy).d - controller.Compute(quiet).d;

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
