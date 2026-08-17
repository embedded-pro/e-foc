#include "core/foc/current_loop/SlidingModeCurrentController.hpp"
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
    constexpr float switchingGain = foc::CurrentLoopTunings{}.switchingGain;
    constexpr float boundaryLayer = foc::CurrentLoopTunings{}.boundaryLayer;

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

    float ExpectedControl(float measured, float reference)
    {
        const float error = measured - reference;
        const float saturated = std::clamp(error / boundaryLayer, -1.0f, 1.0f);

        return Normalized((-(Ad() * error + switchingGain * saturated) + (1.0f - Ad()) * reference) / Bd());
    }

    class TestSlidingModeCurrentController
        : public ::testing::Test
    {
    public:
        float SettledCurrent(float reference)
        {
            const float denormalize = busVoltage / std::numbers::sqrt3_v<float>;
            float current = 0.0f;

            for (int step = 0; step != 500; ++step)
            {
                auto output = controller.Compute({ { current, 0.0f }, { reference, 0.0f }, 0.0f });
                current = Ad() * current + Bd() * output.d * denormalize;
            }

            return current;
        }

        foc::SlidingModeCurrentController controller;
    };
}

TEST_F(TestSlidingModeCurrentController, unconfigured_controller_outputs_zero)
{
    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestSlidingModeCurrentController, on_the_sliding_surface_the_control_is_zero)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestSlidingModeCurrentController, inside_the_boundary_layer_equivalent_and_switching_control_are_summed)
{
    controller.Configure(ValidParameters());

    const float reference = 0.01f;
    auto output = controller.Compute({ { 0.0f, 0.0f }, { reference, -reference }, 0.0f });

    EXPECT_NEAR(output.d, ExpectedControl(0.0f, reference), tolerance);
    EXPECT_NEAR(output.q, ExpectedControl(0.0f, -reference), tolerance);
}

TEST_F(TestSlidingModeCurrentController, current_below_reference_produces_positive_voltage)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_GT(output.d, 0.0f);
    EXPECT_GT(output.q, 0.0f);
}

TEST_F(TestSlidingModeCurrentController, saturated_output_stays_inside_the_modulation_circle)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 100.0f, -100.0f }, 0.0f });

    EXPECT_NEAR(std::hypot(output.d, output.q), 1.0f, tolerance);
    EXPECT_GT(output.d, 0.0f);
    EXPECT_LT(output.q, 0.0f);
}

TEST_F(TestSlidingModeCurrentController, larger_switching_gain_increases_the_control_effort)
{
    controller.Configure(ValidParameters());
    auto reference = controller.Compute({ { 0.0f, 0.0f }, { 0.01f, 0.0f }, 0.0f });

    controller.SetTunings({ 6283.185307f, 2.0f * switchingGain, boundaryLayer, false });
    auto increased = controller.Compute({ { 0.0f, 0.0f }, { 0.01f, 0.0f }, 0.0f });

    EXPECT_GT(increased.d, reference.d);
}

TEST_F(TestSlidingModeCurrentController, non_positive_boundary_layer_disables_the_controller)
{
    controller.Configure(ValidParameters());
    controller.SetTunings({ 6283.185307f, switchingGain, 0.0f, false });

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestSlidingModeCurrentController, default_tunings_settle_on_a_non_zero_reference)
{
    controller.Configure(ValidParameters());
    controller.SetTunings(foc::CurrentLoopTunings{});

    EXPECT_NEAR(SettledCurrent(1.0f), 1.0f, tolerance);
}

TEST_F(TestSlidingModeCurrentController, closed_loop_settles_on_a_non_zero_reference)
{
    controller.Configure(ValidParameters());

    controller.SetTunings({ 6283.185307f, 0.2f, 0.5f, false });

    EXPECT_NEAR(SettledCurrent(1.0f), 1.0f, tolerance);
}
