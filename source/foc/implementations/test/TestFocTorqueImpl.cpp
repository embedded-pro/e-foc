#include "source/foc/implementations/FocTorqueImpl.hpp"
#include <cmath>
#include <gmock/gmock.h>

namespace
{
    class TestFocTorqueImpl
        : public ::testing::Test
    {
    public:
        foc::FocTorqueImpl foc;
    };
}

TEST_F(TestFocTorqueImpl, calculate_returns_valid_pwm_duty_cycles)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians mechanicalAngle{ 0.5f };

    foc.SetPolePairs(4);
    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });
    foc.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });

    auto result = foc.Calculate(currents, mechanicalAngle);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocTorqueImpl, set_current_tunings_scales_by_vdc)
{
    constexpr float invSqrt3 = 0.577350269189625f;
    foc::Volts vdc{ 24.0f };
    float expectedScale = 1.0f / (invSqrt3 * vdc.Value());

    foc::IdAndIqTunings tunings{ { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } };

    foc.SetCurrentTunings(vdc, tunings);

    foc::PhaseCurrents currents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    foc::Radians angle{ 0.0f };

    auto result = foc.Calculate(currents, angle);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocTorqueImpl, pole_pairs_affects_electrical_angle)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians mechanicalAngle{ 0.5f };

    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });
    foc.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });

    foc.SetPolePairs(1);
    auto result1 = foc.Calculate(currents, mechanicalAngle);

    foc.SetPolePairs(4);
    auto result4 = foc.Calculate(currents, mechanicalAngle);

    EXPECT_NE(result1.a.Value(), result4.a.Value());
}

TEST_F(TestFocTorqueImpl, reset_reinitializes_pid_controllers)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.5f };

    foc.SetPolePairs(4);
    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });
    foc.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });

    auto result1 = foc.Calculate(currents, angle);

    foc.Reset();

    auto result2 = foc.Calculate(currents, angle);

    EXPECT_GE(result2.a.Value(), 0);
    EXPECT_LE(result2.a.Value(), 100);
}

TEST_F(TestFocTorqueImpl, setpoint_changes_affect_output)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.5f };

    foc.SetPolePairs(4);
    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });

    foc.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.5f } });
    auto result1 = foc.Calculate(currents, angle);

    foc.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.5f } });
    auto result2 = foc.Calculate(currents, angle);

    EXPECT_NE(result1.a.Value(), result2.a.Value());
}
