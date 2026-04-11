#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"
#include "core/foc/implementations/test_doubles/FocMock.hpp"
#include <gmock/gmock.h>

namespace
{
    using namespace testing;

    class TestWithAutomaticSpeedPidGains
        : public ::testing::Test
    {
    protected:
        ::testing::StrictMock<foc::FocSpeedTunableMock> focMock;
        foc::WithAutomaticSpeedPidGains autoGains{ focMock };
    };
}

TEST_F(TestWithAutomaticSpeedPidGains, kp_equals_inertia_times_bandwidth)
{
    const float inertia = 0.002f;
    const float bandwidth = 50.0f;

    EXPECT_CALL(focMock, SetSpeedTunings(_, _))
        .WillOnce([&](auto, const foc::SpeedTunings& tunings)
            {
                EXPECT_NEAR(tunings.kp, inertia * bandwidth, 1e-6f);
            });

    autoGains.SetPidBasedOnInertiaAndFriction(
        foc::Volts{ 24.0f },
        foc::NewtonMeterSecondSquared{ inertia },
        foc::NewtonMeterSecondPerRadian{ 0.001f },
        bandwidth);
}

TEST_F(TestWithAutomaticSpeedPidGains, ki_equals_friction_times_bandwidth)
{
    const float friction = 0.003f;
    const float bandwidth = 50.0f;

    EXPECT_CALL(focMock, SetSpeedTunings(_, _))
        .WillOnce([&](auto, const foc::SpeedTunings& tunings)
            {
                EXPECT_NEAR(tunings.ki, friction * bandwidth, 1e-6f);
            });

    autoGains.SetPidBasedOnInertiaAndFriction(
        foc::Volts{ 24.0f },
        foc::NewtonMeterSecondSquared{ 0.002f },
        foc::NewtonMeterSecondPerRadian{ friction },
        bandwidth);
}

TEST_F(TestWithAutomaticSpeedPidGains, kd_is_zero)
{
    EXPECT_CALL(focMock, SetSpeedTunings(_, _))
        .WillOnce([](auto, const foc::SpeedTunings& tunings)
            {
                EXPECT_FLOAT_EQ(tunings.kd, 0.0f);
            });

    autoGains.SetPidBasedOnInertiaAndFriction(
        foc::Volts{ 24.0f },
        foc::NewtonMeterSecondSquared{ 0.002f },
        foc::NewtonMeterSecondPerRadian{ 0.001f },
        50.0f);
}
