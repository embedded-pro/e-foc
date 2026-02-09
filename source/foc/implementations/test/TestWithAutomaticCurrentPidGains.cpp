#include "source/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "source/foc/implementations/test_doubles/FocMock.hpp"
#include "gmock/gmock.h"
#include <numbers>

namespace
{
    using namespace testing;

    MATCHER_P2(IdAndIqTuningsNear, expectedKp, expectedKi, "")
    {
        constexpr float tolerance = 1e-4f;
        return std::abs(arg.first.kp - expectedKp) < tolerance &&
               std::abs(arg.first.ki - expectedKi) < tolerance &&
               std::abs(arg.first.kd - 0.0f) < tolerance &&
               std::abs(arg.second.kp - expectedKp) < tolerance &&
               std::abs(arg.second.ki - expectedKi) < tolerance &&
               std::abs(arg.second.kd - 0.0f) < tolerance;
    }

    class TestWithAutomaticPidGains
        : public ::testing::Test
    {
    public:
        ::testing::StrictMock<foc::FocTorqueMock> focMock;
        foc::WithAutomaticCurrentPidGains autoGains{ focMock };
    };
}

TEST_F(TestWithAutomaticPidGains, calculates_pid_gains_from_resistance_and_inductance)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 0.5f };
    foc::MilliHenry inductance{ 1.0f };
    float nyquistFactor = 15.0f;

    float expectedWc = (10000.0f / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.001f * expectedWc;
    float expectedKi = 0.5f * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));
    autoGains.SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, hal::Hertz{ 10000 }, nyquistFactor);
}

TEST_F(TestWithAutomaticPidGains, calculates_pid_gains_with_different_nyquist_factor)
{
    foc::Volts Vdc{ 24.0f };
    foc::Ohm resistance{ 1.0f };
    foc::MilliHenry inductance{ 2.0f };
    float nyquistFactor = 10.0f;

    float expectedWc = (10000.0f / 10.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.002f * expectedWc;
    float expectedKi = 1.0f * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));
    autoGains.SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, hal::Hertz{ 10000 }, nyquistFactor);
}

TEST_F(TestWithAutomaticPidGains, calculates_pid_gains_with_different_base_frequency)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 0.5f };
    foc::MilliHenry inductance{ 1.0f };
    float nyquistFactor = 15.0f;

    float baseFreq = 20000.0f;
    float expectedWc = (baseFreq / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.001f * expectedWc;
    float expectedKi = 0.5f * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));
    autoGains.SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, hal::Hertz{ 20000 }, nyquistFactor);
}

TEST_F(TestWithAutomaticPidGains, calculates_pid_gains_with_small_inductance)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 2.0f };
    foc::MilliHenry inductance{ 0.1f };
    float nyquistFactor = 15.0f;

    float expectedWc = (10000.0f / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.0001f * expectedWc;
    float expectedKi = 2.0f * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));
    autoGains.SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, hal::Hertz{ 10000 }, nyquistFactor);
}

TEST_F(TestWithAutomaticPidGains, calculates_pid_gains_with_high_resistance)
{
    foc::Volts Vdc{ 48.0f };
    foc::Ohm resistance{ 5.0f };
    foc::MilliHenry inductance{ 3.0f };
    float nyquistFactor = 12.0f;

    float expectedWc = (10000.0f / 12.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.003f * expectedWc;
    float expectedKi = 5.0f * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));
    autoGains.SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, hal::Hertz{ 10000 }, nyquistFactor);
}
