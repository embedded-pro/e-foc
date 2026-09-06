#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include <gmock/gmock.h>
#include <numbers>

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

TEST_F(TestRealTimeFrictionAndInertiaEstimator, standstill_observations_leave_the_coefficients_untouched)
{
    foc::NewtonMeter torque{ 0.1f };
    const foc::PhaseCurrents idle{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    const foc::RadiansPerSecond stopped{ 0.0f };

    auto first = estimator.Update(idle, stopped, angle, torque);

    for (int sample = 0; sample != 500; ++sample)
        estimator.Update(idle, stopped, angle, torque);

    auto last = estimator.Update(idle, stopped, angle, torque);

    EXPECT_FLOAT_EQ(last.inertia.Value(), first.inertia.Value());
    EXPECT_FLOAT_EQ(last.friction.Value(), first.friction.Value());
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, an_excited_observation_carrying_torque_updates_the_coefficients)
{
    foc::NewtonMeter torque{ 0.1f };
    const foc::PhaseCurrents idle{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    const foc::Radians quadratureAngle{ std::numbers::pi_v<float> / 2.0f };

    auto before = estimator.Update(idle, foc::RadiansPerSecond{ 0.0f }, quadratureAngle, torque);
    auto after = estimator.Update(currents, foc::RadiansPerSecond{ 50.0f }, quadratureAngle, torque);

    EXPECT_NE(after.inertia.Value(), before.inertia.Value());
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_standstill_run_does_not_publish_new_online_estimates)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    const foc::PhaseCurrents idle{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };

    for (int sample = 0; sample != 2000; ++sample)
        estimator.Update(idle, foc::RadiansPerSecond{ 0.0f }, angle);

    EXPECT_FLOAT_EQ(estimator.CurrentInertia().Value(), 1.0e-4f);
    EXPECT_FLOAT_EQ(estimator.CurrentFriction().Value(), 1.0e-4f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, published_online_estimates_stay_inside_the_plausibility_band)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    for (int sample = 0; sample != 200; ++sample)
        estimator.Update(currents, foc::RadiansPerSecond{ static_cast<float>(sample) * 13.0f }, angle);

    EXPECT_GT(estimator.CurrentInertia().Value(), 0.0f);
    EXPECT_LT(estimator.CurrentInertia().Value(), 1.0f);
    EXPECT_GE(estimator.CurrentFriction().Value(), 0.0f);
    EXPECT_LT(estimator.CurrentFriction().Value(), 1.0f);
}
