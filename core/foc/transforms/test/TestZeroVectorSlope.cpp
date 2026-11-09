#include "core/foc/transforms/ZeroVectorSlope.hpp"
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float resistance{ 0.5f };
    constexpr float inductanceHenry{ 0.2e-3f };
    constexpr float fluxLinkage{ 0.01f };
    constexpr float samplingFrequency{ 20000.0f };

    foc::MotorModelParameters Model(float r = resistance, float lMilliHenry = inductanceHenry * 1000.0f, float psi = fluxLinkage)
    {
        return foc::MotorModelParameters{ foc::Ohm{ r }, foc::MilliHenry{ lMilliHenry }, foc::Weber{ psi }, foc::Volts{ 24.0f }, hal::Hertz{ 20000 }, 4, 1 };
    }

    std::array<float, 3> PhaseRates(float id, float iq, float omega, float theta)
    {
        const auto next = theta + omega / samplingFrequency;
        std::array<float, 3> rates{};
        for (int k = 0; k != 3; ++k)
        {
            const auto shifted = next - 2.0f * std::numbers::pi_v<float> * static_cast<float>(k) / 3.0f;
            const auto current = id * std::cos(shifted) - iq * std::sin(shifted);
            const auto backEmf = -omega * fluxLinkage * std::sin(shifted);
            rates[k] = -(resistance * current + backEmf) / inductanceHenry;
        }
        return rates;
    }

    void ExpectRates(const foc::PhaseCurrentSlopes& actual, const std::array<float, 3>& expected, float tolerance)
    {
        EXPECT_NEAR(actual.a.Value(), expected[0], tolerance);
        EXPECT_NEAR(actual.b.Value(), expected[1], tolerance);
        EXPECT_NEAR(actual.c.Value(), expected[2], tolerance);
    }

    foc::PhaseCurrentSlopes Next(foc::ZeroVectorSlope& slope, float id, float iq, float omega, float theta)
    {
        return slope.Next(foc::RotatingFrame{ id, iq }, omega, std::cos(theta), std::sin(theta));
    }
}

TEST(TestZeroVectorSlope, without_a_model_the_rates_are_zero)
{
    foc::ZeroVectorSlope slope;

    ExpectRates(Next(slope, 1.0f, 2.0f, 1000.0f, 0.3f), { 0.0f, 0.0f, 0.0f }, 0.0f);
}

TEST(TestZeroVectorSlope, a_model_without_inductance_gives_zero_rates)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model(resistance, 0.0f));

    ExpectRates(Next(slope, 1.0f, 2.0f, 1000.0f, 0.3f), { 0.0f, 0.0f, 0.0f }, 0.0f);
}

TEST(TestZeroVectorSlope, at_standstill_each_current_decays_through_the_resistance)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model());

    const auto sqrt3Half = std::numbers::sqrt3_v<float> * 0.5f;
    ExpectRates(Next(slope, 0.0f, 2.0f, 0.0f, 0.0f), { 0.0f, -2500.0f * 2.0f * sqrt3Half, 2500.0f * 2.0f * sqrt3Half }, 0.5f);
}

TEST(TestZeroVectorSlope, without_current_the_back_emf_drives_the_rates_one_period_ahead)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model());

    ExpectRates(Next(slope, 0.0f, 0.0f, 1000.0f, 0.0f), PhaseRates(0.0f, 0.0f, 1000.0f, 0.0f), 30.0f);
}

TEST(TestZeroVectorSlope, the_rates_match_each_phase_winding_equation)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model());

    for (const auto theta : { -2.9f, -1.1f, 0.4f, 2.2f })
        for (const auto omega : { -2500.0f, 0.0f, 1800.0f })
            ExpectRates(Next(slope, -0.7f, 2.5f, omega, theta), PhaseRates(-0.7f, 2.5f, omega, theta), 2.5e-3f * 1.0e5f);
}

TEST(TestZeroVectorSlope, a_tracked_estimate_inside_the_band_replaces_the_resistance_and_inductance)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model(resistance, 0.7f * inductanceHenry * 1000.0f));

    slope.Track(foc::Ohm{ resistance }, foc::MilliHenry{ inductanceHenry * 1000.0f });

    ExpectRates(Next(slope, -0.7f, 2.5f, 1800.0f, 0.4f), PhaseRates(-0.7f, 2.5f, 1800.0f, 0.4f), 250.0f);
}

TEST(TestZeroVectorSlope, an_estimate_outside_the_band_is_not_followed)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model());
    const auto before = Next(slope, -0.7f, 2.5f, 1800.0f, 0.4f);

    slope.Track(foc::Ohm{ resistance }, foc::MilliHenry{ 2.0f * inductanceHenry * 1000.0f });
    slope.Track(foc::Ohm{ 3.0f * resistance }, foc::MilliHenry{ inductanceHenry * 1000.0f });

    ExpectRates(Next(slope, -0.7f, 2.5f, 1800.0f, 0.4f), { before.a.Value(), before.b.Value(), before.c.Value() }, 0.0f);
}

TEST(TestZeroVectorSlope, a_restart_returns_to_the_configured_model)
{
    foc::ZeroVectorSlope slope;
    slope.Configure(Model());
    const auto configured = Next(slope, -0.7f, 2.5f, 1800.0f, 0.4f);

    slope.Track(foc::Ohm{ resistance }, foc::MilliHenry{ 1.2f * inductanceHenry * 1000.0f });
    slope.Restart();

    ExpectRates(Next(slope, -0.7f, 2.5f, 1800.0f, 0.4f), { configured.a.Value(), configured.b.Value(), configured.c.Value() }, 0.0f);
}

TEST(TestZeroVectorSlope, without_a_model_a_tracked_estimate_is_not_followed)
{
    foc::ZeroVectorSlope slope;

    slope.Track(foc::Ohm{ resistance }, foc::MilliHenry{ inductanceHenry * 1000.0f });

    ExpectRates(Next(slope, 1.0f, 2.0f, 1000.0f, 0.3f), { 0.0f, 0.0f, 0.0f }, 0.0f);
}
