#include "core/services/current_controllers/DecoupledPidCurrentController.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1e-4f;
    constexpr float inductanceInHenry = 0.0005f;
    constexpr float fluxLinkage = 0.01f;
    constexpr float busVoltage = 24.0f;

    services::MotorModelParameters ValidParameters()
    {
        return { foc::Ohm{ 1.0f }, foc::MilliHenry{ 0.5f }, foc::Weber{ fluxLinkage }, foc::Volts{ busVoltage }, hal::Hertz{ 20000 } };
    }

    float NormalizedFeedforward(float physicalVoltage)
    {
        return physicalVoltage * std::numbers::sqrt3_v<float> / busVoltage;
    }

    class TestDecoupledPidCurrentController
        : public ::testing::Test
    {
    public:
        services::DecoupledPidCurrentController controller;
    };
}

TEST_F(TestDecoupledPidCurrentController, zero_speed_and_zero_error_produces_zero_output)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestDecoupledPidCurrentController, d_axis_feedforward_cancels_cross_coupling)
{
    controller.Configure(ValidParameters());

    const float electricalSpeed = 100.0f;
    const float iq = 2.0f;
    auto output = controller.Compute({ { 0.0f, iq }, { 0.0f, iq }, electricalSpeed });

    EXPECT_NEAR(output.d, NormalizedFeedforward(-electricalSpeed * inductanceInHenry * iq), tolerance);
}

TEST_F(TestDecoupledPidCurrentController, q_axis_feedforward_adds_coupling_and_back_emf)
{
    controller.Configure(ValidParameters());

    const float electricalSpeed = 100.0f;
    const float id = 2.0f;
    auto output = controller.Compute({ { id, 0.0f }, { id, 0.0f }, electricalSpeed });

    EXPECT_NEAR(output.q, NormalizedFeedforward(electricalSpeed * (inductanceInHenry * id + fluxLinkage)), tolerance);
}

TEST_F(TestDecoupledPidCurrentController, feedforward_is_absent_without_configuration)
{
    auto output = controller.Compute({ { 2.0f, 2.0f }, { 2.0f, 2.0f }, 500.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestDecoupledPidCurrentController, saturated_output_stays_inside_the_modulation_circle)
{
    controller.Configure(ValidParameters());

    auto output = controller.Compute({ { 50.0f, 50.0f }, { 50.0f, 50.0f }, 5000.0f });

    EXPECT_NEAR(std::hypot(output.d, output.q), 1.0f, tolerance);
    EXPECT_LT(output.d, 0.0f);
    EXPECT_GT(output.q, 0.0f);
}

TEST_F(TestDecoupledPidCurrentController, reset_clears_the_embedded_pid_state)
{
    controller.Configure(ValidParameters());
    controller.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    controller.Reset();
    auto output = controller.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}
