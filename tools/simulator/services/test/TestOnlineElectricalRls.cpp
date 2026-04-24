#include "tools/simulator/services/OnlineElectricalRls.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    constexpr float trueR = 2.0f;
    constexpr float trueL = 0.005f;
    constexpr float ts = 1.0e-5f;
    constexpr float omegaElec = 100.0f;
    constexpr int numSamples = 1000;

    class TestOnlineElectricalRls : public ::testing::Test
    {
    protected:
        // Construct a minimal model stub — we do not actually observe; use UpdateForTest
        simulator::ThreePhaseMotorModel MakeModel()
        {
            static const simulator::ThreePhaseMotorModel::Parameters params{
                .R = foc::Ohm{ trueR },
                .Ld = foc::Henry{ trueL },
                .Lq = foc::Henry{ trueL },
                .psi_f = foc::Weber{ 0.007f },
                .p = 4,
                .J = foc::KilogramMeterSquared{ 0.0000075f },
                .B = foc::NewtonMeterSecondPerRadian{ 0.00002f },
            };
            return simulator::ThreePhaseMotorModel{ params, foc::Volts{ 24.0f }, hal::Hertz{ 100000 }, std::optional<std::size_t>{ 1 } };
        }
    };
}

TEST_F(TestOnlineElectricalRls, converges_to_known_resistance_and_inductance)
{
    auto model = MakeModel();
    simulator::OnlineElectricalRls estimator{ model, 4, hal::Hertz{ static_cast<uint32_t>(1.0f / ts) } };

    float id = 0.0f;
    float idPrev = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = static_cast<float>(i) * ts;
        idPrev = id;
        id = 1.0f * std::sin(2.0f * 3.14159265f * 500.0f * t);
        const float iq = 0.5f * std::cos(2.0f * 3.14159265f * 500.0f * t);
        const float didDt = (id - idPrev) / ts;
        const float vd = trueR * id + trueL * didDt - omegaElec * trueL * iq;

        estimator.UpdateForTest(vd, id, iq, omegaElec);
    }

    EXPECT_NEAR(estimator.ResistanceEstimate(), trueR, trueR * 0.10f);
    EXPECT_NEAR(estimator.InductanceEstimate(), trueL, trueL * 0.15f);
}
