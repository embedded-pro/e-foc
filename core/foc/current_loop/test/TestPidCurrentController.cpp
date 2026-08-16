#include "core/foc/current_loop/PidCurrentController.hpp"
#include <cmath>
#include <gmock/gmock.h>

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
