#include "source/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "source/foc/implementations/test_doubles/FocMock.hpp"
#include "gmock/gmock.h"
#include <numbers>

namespace
{
    using namespace testing;

    struct AutoPidGainsParams
    {
        foc::Volts vdc;
        foc::Ohm resistance;
        foc::MilliHenry inductance;
        hal::Hertz baseFrequency;
        float nyquistFactor;
    };

    AutoPidGainsParams MakeParams(float vdc, float resistance, float inductance, unsigned int baseFrequency, float nyquistFactor)
    {
        return { foc::Volts{ vdc }, foc::Ohm{ resistance }, foc::MilliHenry{ inductance }, hal::Hertz{ baseFrequency }, nyquistFactor };
    }

    class TestWithAutomaticPidGains
        : public ::testing::TestWithParam<AutoPidGainsParams>
    {
    public:
        ::testing::StrictMock<foc::FocTorqueMock> focMock;
        foc::WithAutomaticCurrentPidGains autoGains{ focMock };
    };

    // clang-format off
    INSTANTIATE_TEST_SUITE_P(CurrentPidGains, TestWithAutomaticPidGains, ::testing::Values(
        MakeParams(12.0f, 0.5f, 1.0f, 10000, 15.0f),
        MakeParams(24.0f, 1.0f, 2.0f, 10000, 10.0f),
        MakeParams(12.0f, 0.5f, 1.0f, 20000, 15.0f),
        MakeParams(12.0f, 2.0f, 0.1f, 10000, 15.0f),
        MakeParams(48.0f, 5.0f, 3.0f, 10000, 12.0f)
    ));
    // clang-format on
}

TEST_P(TestWithAutomaticPidGains, kp_equals_inductance_in_henry_times_bandwidth)
{
    const auto& p = GetParam();

    float expectedWc = (static_cast<float>(p.baseFrequency.Value()) / p.nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = p.inductance.Value() * 0.001f * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(p.vdc), ::testing::_))
        .WillOnce([&](auto, const foc::IdAndIqTunings& tunings)
            {
                EXPECT_NEAR(tunings.first.kp, expectedKp, 1e-4f);
                EXPECT_NEAR(tunings.second.kp, expectedKp, 1e-4f);
            });

    autoGains.SetPidBasedOnResistanceAndInductance(p.vdc, p.resistance, p.inductance, p.baseFrequency, p.nyquistFactor);
}

TEST_P(TestWithAutomaticPidGains, ki_equals_resistance_times_bandwidth)
{
    const auto& p = GetParam();

    float expectedWc = (static_cast<float>(p.baseFrequency.Value()) / p.nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
    float expectedKi = p.resistance.Value() * expectedWc;

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(p.vdc), ::testing::_))
        .WillOnce([&](auto, const foc::IdAndIqTunings& tunings)
            {
                EXPECT_NEAR(tunings.first.ki, expectedKi, 1e-4f);
                EXPECT_NEAR(tunings.second.ki, expectedKi, 1e-4f);
            });

    autoGains.SetPidBasedOnResistanceAndInductance(p.vdc, p.resistance, p.inductance, p.baseFrequency, p.nyquistFactor);
}

TEST_P(TestWithAutomaticPidGains, kd_is_zero)
{
    const auto& p = GetParam();

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(p.vdc), ::testing::_))
        .WillOnce([](auto, const foc::IdAndIqTunings& tunings)
            {
                EXPECT_FLOAT_EQ(tunings.first.kd, 0.0f);
                EXPECT_FLOAT_EQ(tunings.second.kd, 0.0f);
            });

    autoGains.SetPidBasedOnResistanceAndInductance(p.vdc, p.resistance, p.inductance, p.baseFrequency, p.nyquistFactor);
}

TEST_P(TestWithAutomaticPidGains, id_and_iq_tunings_are_identical)
{
    const auto& p = GetParam();

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(p.vdc), ::testing::_))
        .WillOnce([](auto, const foc::IdAndIqTunings& tunings)
            {
                EXPECT_FLOAT_EQ(tunings.first.kp, tunings.second.kp);
                EXPECT_FLOAT_EQ(tunings.first.ki, tunings.second.ki);
                EXPECT_FLOAT_EQ(tunings.first.kd, tunings.second.kd);
            });

    autoGains.SetPidBasedOnResistanceAndInductance(p.vdc, p.resistance, p.inductance, p.baseFrequency, p.nyquistFactor);
}
