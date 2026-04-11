#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include <gmock/gmock.h>

namespace
{
    class TestRealTimeFrictionAndInertiaEstimator
        : public ::testing::Test
    {
    public:
        services::RealTimeFrictionAndInertiaEstimator estimator{ 0.99f, hal::Hertz{ 1000 } };

        foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
        foc::RadiansPerSecond speed{ 10.0f };
        foc::Radians angle{ 0.0f };
    };
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, update_produces_finite_results)
{
    foc::NewtonMeter torque{ 0.1f };

    auto result = estimator.Update(currents, speed, angle, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
    EXPECT_TRUE(std::isfinite(result.metrics.innovation));
    EXPECT_TRUE(std::isfinite(result.metrics.residual));
    EXPECT_TRUE(std::isfinite(result.metrics.uncertainty));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, acceleration_calculated_from_speed_difference)
{
    foc::Radians a{ 0.0f };
    foc::NewtonMeter torque{ 0.1f };

    estimator.Update(currents, foc::RadiansPerSecond{ 10.0f }, a, torque);
    auto result = estimator.Update(currents, foc::RadiansPerSecond{ 20.0f }, a, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, result_units_are_correct_types)
{
    foc::NewtonMeter torque{ 0.15f };

    auto result = estimator.Update(currents, foc::RadiansPerSecond{ 50.0f }, foc::Radians{ 0.5f }, torque);

    foc::NewtonMeterSecondSquared inertia = result.inertia;
    foc::NewtonMeterSecondPerRadian friction = result.friction;

    EXPECT_TRUE(std::isfinite(inertia.Value()));
    EXPECT_TRUE(std::isfinite(friction.Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, set_initial_estimate_stores_values)
{
    estimator.SetInitialEstimate(
        foc::NewtonMeterSecondSquared{ 0.002f },
        foc::NewtonMeterSecondPerRadian{ 0.001f });

    EXPECT_FLOAT_EQ(estimator.CurrentInertia().Value(), 0.002f);
    EXPECT_FLOAT_EQ(estimator.CurrentFriction().Value(), 0.001f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, three_param_update_produces_finite_estimates)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });

    estimator.Update(currents, speed, angle);
    estimator.Update(currents, foc::RadiansPerSecond{ 15.0f }, angle);

    EXPECT_TRUE(std::isfinite(estimator.CurrentInertia().Value()));
    EXPECT_TRUE(std::isfinite(estimator.CurrentFriction().Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, set_torque_constant_affects_update)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.05f });
    estimator.Update(currents, speed, angle);

    EXPECT_TRUE(std::isfinite(estimator.CurrentInertia().Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, set_initial_estimate_seeds_rls_so_values_persist_under_no_excitation)
{
    // Arrange: seed with known values.
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(
        foc::NewtonMeterSecondSquared{ 0.01f },
        foc::NewtonMeterSecondPerRadian{ 0.005f });

    // Act: update with zero-current and constant-speed data.
    // With Iq=0, electromagnetic torque=0, so output=0.
    // With constant speed (acc=0), regressor=[1,0,0], prediction=theta[0]=0 (coulomb).
    // RLS error=0 → theta unchanged → seeded values should persist.
    foc::PhaseCurrents zeroCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    estimator.Update(zeroCurrents, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.0f });
    estimator.Update(zeroCurrents, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.0f });

    // Assert: estimates remain near seeded values, not the zero-initialised RLS default.
    EXPECT_NEAR(estimator.CurrentInertia().Value(), 0.01f, 0.005f);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), 0.005f, 0.003f);
}
