#include "tools/simulator/services/OnlineMechanicalRls.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    constexpr float trueB = 1.0e-4f;
    constexpr float trueJ = 1.0e-5f;
    constexpr float dt = 1.0e-3f;
    constexpr int numSamples = 1000;

    class TestOnlineMechanicalRls : public ::testing::Test
    {
    protected:
        simulator::ThreePhaseMotorModel MakeModel()
        {
            static const simulator::ThreePhaseMotorModel::Parameters params{
                .R = foc::Ohm{ 2.0f },
                .Ld = foc::Henry{ 0.005f },
                .Lq = foc::Henry{ 0.005f },
                .psi_f = foc::Weber{ 0.007f },
                .p = 4,
                .J = foc::KilogramMeterSquared{ trueJ },
                .B = foc::NewtonMeterSecondPerRadian{ trueB },
            };
            return simulator::ThreePhaseMotorModel{ params, foc::Volts{ 24.0f }, hal::Hertz{ 100000 }, std::optional<std::size_t>{ 1 } };
        }
    };
}

TEST_F(TestOnlineMechanicalRls, converges_to_known_friction_and_inertia)
{
    auto model = MakeModel();
    simulator::ThreePhaseMotorModel::Parameters params{
        .R = foc::Ohm{ 2.0f },
        .Ld = foc::Henry{ 0.005f },
        .Lq = foc::Henry{ 0.005f },
        .psi_f = foc::Weber{ 0.007f },
        .p = 4,
        .J = foc::KilogramMeterSquared{ trueJ },
        .B = foc::NewtonMeterSecondPerRadian{ trueB },
    };

    simulator::OnlineElectricalRls electricalRls{ model, 4, hal::Hertz{ 1000 } };
    // decimation=1 so every sample is fed
    simulator::OnlineMechanicalRls estimator{ model, electricalRls, params, hal::Hertz{ static_cast<uint32_t>(1.0f / dt) }, 1 };

    float omega = 0.0f;
    float omegaPrev = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = static_cast<float>(i) * dt;
        omegaPrev = omega;
        omega = 10.0f * std::sin(2.0f * 3.14159265f * 2.0f * t);
        const float domegaDt = (omega - omegaPrev) / dt;
        const float te = trueJ * domegaDt + trueB * omega;

        estimator.UpdateForTest(te, omega, dt);
    }

    EXPECT_NEAR(estimator.FrictionEstimate(), trueB, trueB * 0.15f);
    EXPECT_NEAR(estimator.InertiaEstimate(), trueJ, trueJ * 0.15f);
}
