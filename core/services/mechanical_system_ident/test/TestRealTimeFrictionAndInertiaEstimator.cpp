#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/math/MechanicalWindowAccumulator.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>
#include <random>

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
        float dryFriction{ 0.0f };
        float currentNoise{ 0.0f };
        float torqueDisturbance{ 0.0f };
        float cogging{ 0.0f };
        float integralBandwidth{ 0.0f };
        bool frictionFeedForward{ true };
    };

    constexpr float coggingCyclesPerRevolution{ 24.0f };

    constexpr uint32_t substeps{ 20 };
    constexpr float currentLoopTimeConstant{ 1e-4f };

    // A rotor under a speed loop, proportional with the rotor's friction fed forward or proportional-integral without it,
    // sampled substeps times per outer-loop window through an encoder of the given
    // counts (zero for a continuous one), the way the control interrupt accumulates its windows. The current follows
    // its reference through a current loop, as a winding's does, and the position and the current are sampled at the
    // same instant, the start of each step. The sampled current can carry white noise, and the shaft a torque
    // disturbance the model does not know of, drawn anew for each window, and a cogging torque that follows the angle.
    class WindowedRotorRun
    {
    public:
        WindowedRotorRun(const ReferenceRotor& rotor, services::RealTimeFrictionAndInertiaEstimator& estimator, uint32_t countsPerRevolution = 0)
            : rotor{ rotor }
            , estimator{ estimator }
            , count{ countsPerRevolution != 0 ? 2.0f * std::numbers::pi_v<float> / static_cast<float>(countsPerRevolution) : 0.0f }
        {}

        void Run(float lowSpeed, float highSpeed, int samplesPerLevel, int samples)
        {
            Run(std::array<float, 2>{ lowSpeed, highSpeed }, samplesPerLevel, samples);
        }

        template<std::size_t Levels>
        void Run(const std::array<float, Levels>& levels, int samplesPerLevel, int samples)
        {
            constexpr float dt{ 1.0f / outerLoopFrequency / substeps };
            const float gain{ 2.0f * rotor.inertia * 200.0f / rotor.torqueConstant };
            const float currentResponse{ 1.0f - std::exp(-dt / currentLoopTimeConstant) };

            for (int sample = 0; sample != samples; ++sample)
            {
                const auto reference = levels[static_cast<std::size_t>(sample / samplesPerLevel) % Levels];
                const auto feedForward = rotor.frictionFeedForward ? (rotor.friction * reference + std::copysign(rotor.dryFriction, reference)) / rotor.torqueConstant : 0.0f;
                integratedError += (reference - measuredSpeed) / outerLoopFrequency;
                const auto currentReference = gain * (reference - measuredSpeed) + gain * rotor.integralBandwidth * integratedError + feedForward;

                const auto startAngle = measuredAngle;
                const auto disturbance = noise(engine) * rotor.torqueDisturbance;
                for (uint32_t substep = 0; substep != substeps; ++substep)
                {
                    const auto measured = Measure(angle);
                    window.Accumulate(current + noise(engine) * rotor.currentNoise, measured - measuredAngle);
                    measuredAngle = measured;

                    const auto acceleration = (rotor.torqueConstant * current - rotor.friction * speed - std::copysign(rotor.dryFriction, speed) - rotor.shaftTorque - disturbance - rotor.cogging * std::sin(coggingCyclesPerRevolution * angle)) / rotor.inertia;
                    angle += speed * dt + 0.5f * acceleration * dt * dt;
                    speed += acceleration * dt;
                    current += (currentReference - current) * currentResponse;
                }

                measuredSpeed = (measuredAngle - startAngle) * outerLoopFrequency;
                estimator.Update(window.Close());
            }
        }

    private:
        float Measure(float trueAngle) const
        {
            return count > 0.0f ? std::floor(trueAngle / count) * count : trueAngle;
        }

        ReferenceRotor rotor;
        services::RealTimeFrictionAndInertiaEstimator& estimator;
        float count;
        foc::MechanicalWindowAccumulator window{ substeps };
        std::mt19937 engine{ 7 };
        std::normal_distribution<float> noise{ 0.0f, 1.0f };
        float current{ 0.0f };
        float speed{ 0.0f };
        float angle{ 0.0f };
        float measuredAngle{ 0.0f };
        float measuredSpeed{ 0.0f };
        float integratedError{ 0.0f };
    };

    foc::MechanicalWindow SteadyWindow(float iq, float speed, bool first)
    {
        foc::MechanicalWindowAccumulator window{ substeps };
        const auto step = speed / outerLoopFrequency / static_cast<float>(substeps);
        for (uint32_t substep = 0; substep != substeps; ++substep)
            window.Accumulate(iq, step);

        auto closed = window.Close();
        closed.first = first;
        return closed;
    }
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

    EXPECT_NEAR(estimator.CurrentInertia().Value(), 0.002f, 1e-9f);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), 0.001f, 1e-9f);
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

    EXPECT_NEAR(last.inertia.Value(), first.inertia.Value(), 1e-9f);
    EXPECT_NEAR(last.friction.Value(), first.friction.Value(), 1e-9f);
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
        estimator.Update(SteadyWindow(0.0f, 0.0f, sample == 0));

    EXPECT_NEAR(estimator.CurrentInertia().Value(), 1.0e-4f, 1e-9f);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), 1.0e-4f, 1e-9f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_steady_speed_is_not_excitation_enough_to_publish)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    for (int sample = 0; sample != 5000; ++sample)
        estimator.Update(SteadyWindow(0.5f, speed.Value(), sample == 0));

    EXPECT_NEAR(estimator.CurrentInertia().Value(), 1.0e-4f, 1e-9f);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), 1.0e-4f, 1e-9f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_short_burst_of_excitation_is_not_enough_to_publish)
{
    estimator.SetTorqueConstant(foc::NewtonMeter{ 0.1f });
    estimator.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f });

    for (int sample = 0; sample != 8; ++sample)
        estimator.Update(SteadyWindow(1.0f, static_cast<float>(sample) * 13.0f, sample == 0));

    EXPECT_NEAR(estimator.CurrentInertia().Value(), 1.0e-4f, 1e-9f);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), 1.0e-4f, 1e-9f);
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

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_constant_shaft_torque_in_one_direction_is_absorbed_by_the_dry_friction_term)
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

    EXPECT_NEAR(estimator.CurrentInertia().Value(), 1.0e-4f, 1e-9f);
    EXPECT_NEAR(estimator.CurrentFriction().Value(), 1.0e-4f, 1e-9f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_four_thousand_count_encoder_identifies_the_reference_rotor_from_a_wrong_seed)
{
    services::RealTimeFrictionAndInertiaEstimator production{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    const ReferenceRotor rotor;
    production.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    production.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun{ rotor, production, 4000 }.Run(26.0f, 52.0f, 250, 6000);

    EXPECT_NEAR(production.CurrentInertia().Value(), rotor.inertia, 0.02f * rotor.inertia);
    EXPECT_NEAR(production.CurrentFriction().Value(), rotor.friction, 0.05f * rotor.friction);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, the_bench_rotor_is_identified_through_an_unmodelled_torque_disturbance)
{
    services::RealTimeFrictionAndInertiaEstimator production{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    ReferenceRotor rotor;
    rotor.inertia = 13.9e-6f;
    rotor.friction = 7e-6f;
    rotor.torqueConstant = 0.042f;
    rotor.shaftTorque = 3.7e-3f;
    rotor.currentNoise = 0.012f;
    rotor.torqueDisturbance = 5e-3f;
    production.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    production.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun{ rotor, production, 4000 }.Run(26.0f, 52.0f, 100, 3200);

    EXPECT_NEAR(production.CurrentInertia().Value(), rotor.inertia, 0.02f * rotor.inertia);
    EXPECT_NEAR(production.CurrentFriction().Value(), rotor.friction, 0.3f * rotor.friction);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_dry_friction_that_turns_with_the_rotor_is_not_read_as_viscous_friction)
{
    services::RealTimeFrictionAndInertiaEstimator production{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    ReferenceRotor rotor;
    rotor.inertia = 13.9e-6f;
    rotor.friction = 7e-6f;
    rotor.torqueConstant = 0.042f;
    rotor.dryFriction = 3.7e-3f;
    production.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    production.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun{ rotor, production, 4000 }.Run(std::array<float, 4>{ 30.0f, 60.0f, -30.0f, -60.0f }, 500, 8000);

    EXPECT_NEAR(production.CurrentInertia().Value(), rotor.inertia, 0.02f * rotor.inertia);
    EXPECT_NEAR(production.CurrentFriction().Value(), rotor.friction, 0.1f * rotor.friction);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, reversing_at_one_speed_cannot_tell_dry_from_viscous_friction_and_publishes_neither)
{
    services::RealTimeFrictionAndInertiaEstimator production{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    ReferenceRotor rotor;
    rotor.dryFriction = 3.7e-3f;
    production.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    production.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun{ rotor, production, 4000 }.Run(std::array<float, 2>{ 30.0f, -30.0f }, 500, 8000);

    EXPECT_NEAR(production.CurrentFriction().Value(), 0.5f * rotor.friction, 1e-9f);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_shaft_torque_that_turns_the_rotor_backwards_before_the_excitation_is_not_read_as_viscous_friction)
{
    services::RealTimeFrictionAndInertiaEstimator production{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    ReferenceRotor rotor;
    rotor.shaftTorque = 0.01f;
    production.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    production.SetInitialEstimate(foc::NewtonMeterSecondSquared{ 2.0f * rotor.inertia }, foc::NewtonMeterSecondPerRadian{ 0.5f * rotor.friction });

    WindowedRotorRun run{ rotor, production };
    run.Run(std::array<float, 1>{ 0.0f }, 700, 700);
    run.Run(26.0f, 52.0f, 250, 6000);

    EXPECT_NEAR(production.CurrentInertia().Value(), rotor.inertia, 0.02f * rotor.inertia);
    EXPECT_NEAR(production.CurrentFriction().Value(), rotor.friction, 0.15f * rotor.friction);
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, a_stop_through_zero_after_the_speed_levels_does_not_move_the_viscous_friction)
{
    services::RealTimeFrictionAndInertiaEstimator production{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    ReferenceRotor rotor;
    rotor.inertia = 13.9e-6f;
    rotor.friction = 10e-6f;
    rotor.torqueConstant = 0.042f;
    rotor.dryFriction = 3.7e-3f;
    rotor.cogging = 4e-3f;
    rotor.integralBandwidth = 40.0f;
    rotor.frictionFeedForward = false;
    production.SetTorqueConstant(foc::NewtonMeter{ rotor.torqueConstant });
    production.SetInitialEstimate(foc::NewtonMeterSecondSquared{ rotor.inertia }, foc::NewtonMeterSecondPerRadian{ rotor.friction });

    WindowedRotorRun run{ rotor, production, 4000 };
    run.Run(std::array<float, 2>{ 30.0f, 60.0f }, 2500, 5000);
    const auto afterTheLevels = production.CurrentFriction().Value();
    run.Run(std::array<float, 2>{ -30.0f, 0.0f }, 2500, 5000);

    EXPECT_NEAR(afterTheLevels, rotor.friction, 0.15f * rotor.friction);
    EXPECT_NEAR(production.CurrentFriction().Value(), afterTheLevels, 0.02f * afterTheLevels);
}
