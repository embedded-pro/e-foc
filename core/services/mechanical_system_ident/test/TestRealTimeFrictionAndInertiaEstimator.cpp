#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float outerLoopFrequency = 1000.0f;
    constexpr float torqueConstant = 0.1f;

    class TestRealTimeFrictionAndInertiaEstimator
        : public ::testing::Test
    {
    public:
        services::RealTimeFrictionAndInertiaEstimator estimator{ 0.99f, hal::Hertz{ 1000 } };

        foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
        foc::RadiansPerSecond speed{ 10.0f };
        foc::Radians angle{ 0.0f };

        float previousSpeed{ 0.0f };

        // Builds the phase currents whose q component, after the estimator's own Clarke/Park at this
        // electrical angle, carries exactly the requested torque.
        foc::PhaseCurrents CurrentsProducing(float torque, foc::Radians electricalAngle) const
        {
            const auto cosine = foc::FastTrigonometry::Cosine(electricalAngle.Value());
            const auto sine = foc::FastTrigonometry::Sine(electricalAngle.Value());
            const foc::ClarkePark transform;

            const auto unitGain = transform.Forward(transform.Inverse(foc::RotatingFrame{ 0.0f, 1.0f }, cosine, sine), cosine, sine).q;
            const auto phases = transform.Inverse(foc::RotatingFrame{ 0.0f, torque / torqueConstant / unitGain }, cosine, sine);

            return foc::PhaseCurrents{ foc::Ampere{ phases.a }, foc::Ampere{ phases.b }, foc::Ampere{ phases.c } };
        }
    };

    struct ReferenceRotor
    {
        float inertia{ 7.06e-6f };
        float friction{ 1.5e-5f };
        float torqueConstant{ 0.0384f };
        float shaftTorque{ 0.0f };
    };

    class WindowedRotorRun
    {
    public:
        WindowedRotorRun(const ReferenceRotor& rotor, services::RealTimeFrictionAndInertiaEstimator& estimator)
            : rotor{ rotor }
            , estimator{ estimator }
        {}

        void Run(float lowSpeed, float highSpeed, int samplesPerLevel, int samples)
        {
            constexpr int substeps{ 20 };
            constexpr float dt{ 1.0f / outerLoopFrequency / substeps };
            const float gain{ 2.0f * rotor.inertia * 200.0f / rotor.torqueConstant };

            for (int sample = 0; sample != samples; ++sample)
            {
                const auto reference = (sample / samplesPerLevel) % 2 == 0 ? lowSpeed : highSpeed;
                const auto current = pendingCurrent;
                pendingCurrent = gain * (reference - measuredSpeed) + rotor.friction * reference / rotor.torqueConstant;

                const auto startAngle = angle;
                for (int substep = 0; substep != substeps; ++substep)
                {
                    speed += (rotor.torqueConstant * current - rotor.friction * speed - rotor.shaftTorque) / rotor.inertia * dt;
                    angle += speed * dt;
                }

                measuredSpeed = (angle - startAngle) * outerLoopFrequency;
                estimator.Update(foc::MechanicalWindow{ foc::Ampere{ current }, foc::RadiansPerSecond{ measuredSpeed } });
            }
        }

    private:
        ReferenceRotor rotor;
        services::RealTimeFrictionAndInertiaEstimator& estimator;
        float pendingCurrent{ 0.0f };
        float speed{ 0.0f };
        float angle{ 0.0f };
        float measuredSpeed{ 0.0f };
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

    for (int sample = 0; sample != 2000; ++sample)
        estimator.Update(foc::MechanicalWindow{ foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_FLOAT_EQ(estimator.CurrentInertia().Value(), 1.0e-4f);
    EXPECT_FLOAT_EQ(estimator.CurrentFriction().Value(), 1.0e-4f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_steady_speed_is_not_excitation_enough_to_publish)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    for (int sample = 0; sample != 5000; ++sample)
        estimator.Update(foc::MechanicalWindow{ foc::Ampere{ 0.5f }, speed });

    EXPECT_FLOAT_EQ(estimator.CurrentInertia().Value(), 1.0e-4f);
    EXPECT_FLOAT_EQ(estimator.CurrentFriction().Value(), 1.0e-4f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_short_burst_of_excitation_is_not_enough_to_publish)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    for (int sample = 0; sample != 8; ++sample)
        estimator.Update(foc::MechanicalWindow{ foc::Ampere{ 1.0f }, foc::RadiansPerSecond{ static_cast<float>(sample) * 13.0f } });

    EXPECT_FLOAT_EQ(estimator.CurrentInertia().Value(), 1.0e-4f);
    EXPECT_FLOAT_EQ(estimator.CurrentFriction().Value(), 1.0e-4f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_two_level_speed_reference_identifies_the_reference_rotor_from_a_wrong_seed)
{
    const ReferenceRotor rotor;
    estimator.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun{ rotor, estimator }.Run(26.0f, 52.0f, 250, 6000);

    EXPECT_NEAR(estimator.CurrentInertia().Value(), rotor.inertia, 0.05f * rotor.inertia);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), rotor.friction, 0.1f * rotor.friction);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_constant_shaft_torque_is_absorbed_by_the_intercept)
{
    ReferenceRotor rotor;
    rotor.shaftTorque = 0.01f;
    estimator.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun{ rotor, estimator }.Run(26.0f, 52.0f, 250, 6000);

    EXPECT_NEAR(estimator.CurrentInertia().Value(), rotor.inertia, 0.05f * rotor.inertia);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), rotor.friction, 0.1f * rotor.friction);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, reseeding_restarts_the_online_fit_from_the_new_seed)
{
    const ReferenceRotor rotor;
    estimator.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });
    WindowedRotorRun{ rotor, estimator }.Run(26.0f, 52.0f, 250, 3000);

    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    EXPECT_FLOAT_EQ(estimator.CurrentInertia().Value(), 1.0e-4f);
    EXPECT_FLOAT_EQ(estimator.CurrentFriction().Value(), 1.0e-4f);
}
