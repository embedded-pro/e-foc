#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include "numerical/math/FastTrigonometry.hpp"
#include <array>
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    const foc::PhasePwmDutyCycles kNeutralDuty{
        hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(40)
    };

    class RecordingObserver
        : public foc::ThreePhaseMotorModelObserver
    {
    public:
        explicit RecordingObserver(foc::ThreePhaseMotorModel& model)
            : ThreePhaseMotorModelObserver(model)
        {}

        void Started() override
        {
            ++startCount;
        }

        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents, foc::Radians, foc::RadiansPerSecond) override
        {
            ++phaseCurrentCount;
        }

        void StatorVoltages(foc::ThreePhase, foc::TwoPhase) override
        {
            ++statorVoltageCount;
        }

        int startCount{};
        int phaseCurrentCount{};
        int statorVoltageCount{};
    };

    class MotorModelTest
        : public ::testing::Test
    {
    protected:
        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 20000 }
        };
    };
}

TEST_F(MotorModelTest, base_frequency_returns_configured_value)
{
    EXPECT_EQ(model.BaseFrequency(), hal::Hertz{ 20000 });
}

TEST_F(MotorModelTest, max_current_supported_is_positive)
{
    EXPECT_GT(model.MaxCurrentSupported().Value(), 0.0f);
}

TEST_F(MotorModelTest, start_notifies_observer_started)
{
    RecordingObserver obs{ model };
    model.Start();
    model.Stop();
    EXPECT_EQ(obs.startCount, 1);
}

TEST_F(MotorModelTest, start_when_already_running_is_idempotent)
{
    RecordingObserver obs{ model };
    model.Start();
    model.Start();
    model.Stop();
    EXPECT_EQ(obs.startCount, 1);
}

TEST_F(MotorModelTest, stop_resets_state)
{
    model.Start();
    model.Stop();
    RecordingObserver obs{ model };
    model.Start();
    model.Stop();
    EXPECT_EQ(obs.startCount, 1);
}

TEST_F(MotorModelTest, step_notifies_phase_currents_and_stator_voltages)
{
    RecordingObserver obs{ model };
    model.StepForTest(kNeutralDuty);
    EXPECT_EQ(obs.phaseCurrentCount, 1);
    EXPECT_EQ(obs.statorVoltageCount, 1);
}

TEST_F(MotorModelTest, phase_currents_ready_callback_invoked_per_step)
{
    int callbackCount = 0;
    model.PhaseCurrentsReady(hal::Hertz{ 20000 }, [&callbackCount](foc::PhaseCurrents)
        {
            ++callbackCount;
        });

    model.StepForTest(kNeutralDuty);
    model.StepForTest(kNeutralDuty);

    EXPECT_EQ(callbackCount, 2);
}

TEST_F(MotorModelTest, set_and_read_angle_round_trips)
{
    model.Encoder().Set(foc::Radians{ 1.5f });
    EXPECT_NEAR(model.Encoder().Read().Value(), 1.5f, 1e-6f);
}

TEST_F(MotorModelTest, set_zero_resets_angle)
{
    model.Encoder().Set(foc::Radians{ 2.0f });
    model.Encoder().SetZero();
    EXPECT_NEAR(model.Encoder().Read().Value(), 0.0f, 1e-6f);
}

TEST_F(MotorModelTest, set_load_does_not_crash)
{
    model.Mechanics().SetLoad(foc::NewtonMeter{ 0.1f });
    model.StepForTest(kNeutralDuty);
}

TEST_F(MotorModelTest, effective_inductance_d_at_ambient_equals_ld)
{
    const float expected = foc::M_2310P_LN_04K::parameters.Ld.Value();
    EXPECT_NEAR(foc::Henry{ model.Winding().inductanceD }.Value(), expected, 1e-9f);
}

TEST_F(MotorModelTest, effective_inductance_q_at_ambient_equals_lq)
{
    const float expected = foc::M_2310P_LN_04K::parameters.Lq.Value();
    EXPECT_NEAR(foc::Henry{ model.Winding().inductanceQ }.Value(), expected, 1e-9f);
}

TEST_F(MotorModelTest, pwm_output_steps_the_plant_one_period)
{
    RecordingObserver obs{ model };
    int callbackCount = 0;
    model.PhaseCurrentsReady(hal::Hertz{ 20000 }, [&callbackCount](foc::PhaseCurrents)
        {
            ++callbackCount;
        });

    model.ThreePhasePwmOutput(kNeutralDuty);

    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(obs.phaseCurrentCount, 1);
}

TEST_F(MotorModelTest, stop_releases_the_phase_currents_callback)
{
    int callbackCount = 0;
    model.PhaseCurrentsReady(hal::Hertz{ 20000 }, [&callbackCount](foc::PhaseCurrents)
        {
            ++callbackCount;
        });

    model.Stop();
    model.ThreePhasePwmOutput(kNeutralDuty);

    EXPECT_EQ(callbackCount, 0);
}

TEST_F(MotorModelTest, thermal_config_applied_changes_effective_resistance)
{
    foc::ThreePhaseMotorModel::ThermalConfig cfg{};
    cfg.copperTempCoeff = 0.00393f;
    model.Thermal().Configure(cfg);
    model.Thermal().SetTemperature(125.0f);

    const float r0 = foc::M_2310P_LN_04K::parameters.R.Value();
    EXPECT_GT(foc::Ohm{ model.Winding().resistance }.Value(), r0);
}

namespace
{
    const foc::PhasePwmDutyCycles kZeroVoltageDuty{
        hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50)
    };

    constexpr int kFreeRunSteps = 100;
}

TEST_F(MotorModelTest, external_torque_defaults_to_zero)
{
    EXPECT_NEAR(model.Mechanics().ExternalTorque().Value(), 0.0f, 1e-6f);
}

TEST_F(MotorModelTest, free_rotor_without_external_torque_stays_at_rest)
{
    for (int i = 0; i != kFreeRunSteps; ++i)
        model.StepForTest(kZeroVoltageDuty);

    EXPECT_NEAR(model.Mechanics().Speed().Value(), 0.0f, 1e-6f);
    EXPECT_NEAR(model.Mechanics().Angle().Value(), 0.0f, 1e-6f);
}

TEST_F(MotorModelTest, negative_external_torque_accelerates_a_free_rotor)
{
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ -0.001f });

    for (int i = 0; i != kFreeRunSteps; ++i)
        model.StepForTest(kZeroVoltageDuty);

    EXPECT_GT(model.Mechanics().Speed().Value(), 0.1f);
    EXPECT_GT(model.Mechanics().Angle().Value(), 0.0f);
}

TEST_F(MotorModelTest, positive_external_torque_drives_a_free_rotor_backwards)
{
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ 0.001f });

    for (int i = 0; i != kFreeRunSteps; ++i)
        model.StepForTest(kZeroVoltageDuty);

    EXPECT_LT(model.Mechanics().Speed().Value(), -0.1f);
    EXPECT_LT(model.Mechanics().Angle().Value(), 0.0f);
}

TEST_F(MotorModelTest, external_torque_survives_start)
{
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ 0.002f });
    model.Start();
    EXPECT_NEAR(model.Mechanics().ExternalTorque().Value(), 0.002f, 1e-9f);
    model.Stop();
}

TEST_F(MotorModelTest, last_dq_currents_are_zero_before_any_step)
{
    EXPECT_NEAR(model.Electrical().LastDqCurrents().d, 0.0f, 1e-6f);
    EXPECT_NEAR(model.Electrical().LastDqCurrents().q, 0.0f, 1e-6f);
}

TEST_F(MotorModelTest, last_dq_currents_follow_a_driven_step)
{
    model.StepForTest(kNeutralDuty);

    const auto dq = model.Electrical().LastDqCurrents();
    EXPECT_NE(std::abs(dq.d) + std::abs(dq.q), 0.0f);
}

TEST_F(MotorModelTest, the_phase_currents_read_back_in_the_frame_the_rotor_has_reached)
{
    const foc::ClarkePark transform;
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ -0.05f });

    for (int step = 0; step != 4000; ++step)
        model.StepForTest(kZeroVoltageDuty);

    const auto electricalAngle = static_cast<float>(foc::M_2310P_LN_04K::parameters.p) * model.Mechanics().Angle().Value();
    const auto phases = model.CurrentSensor().Last();
    const auto dq = transform.Forward(foc::ThreePhase{ phases.a.Value(), phases.b.Value(), phases.c.Value() },
        math::FastCosine(electricalAngle), math::FastSine(electricalAngle));
    const auto modelled = model.Electrical().LastDqCurrents();

    ASSERT_GT(std::abs(model.Mechanics().Speed().Value()), 5.0f);
    EXPECT_NEAR(dq.d, modelled.d, 1e-3f * std::hypot(modelled.d, modelled.q));
    EXPECT_NEAR(dq.q, modelled.q, 1e-3f * std::hypot(modelled.d, modelled.q));
}

TEST_F(MotorModelTest, the_rotor_accelerates_with_the_current_reported_at_the_same_sample)
{
    constexpr int kSteps = 40;
    constexpr float kTorqueConstantFactor = 1.5f;
    const auto& parameters = foc::M_2310P_LN_04K::parameters;
    const float dt = 1.0f / 20000.0f;

    std::array<float, kSteps> angles{};
    std::array<float, kSteps> currents{};
    std::array<float, kSteps> speeds{};

    for (int step = 0; step != kSteps; ++step)
    {
        model.StepForTest(kNeutralDuty);
        angles[step] = model.Mechanics().Angle().Value();
        currents[step] = model.Electrical().LastDqCurrents().q;
        speeds[step] = model.Mechanics().Speed().Value();
    }

    for (int step = 1; step != kSteps - 1; ++step)
    {
        const auto torque = kTorqueConstantFactor * static_cast<float>(parameters.p) * parameters.psi_f.Value() * currents[step];
        const auto expected = (torque - parameters.B.Value() * speeds[step]) / parameters.J.Value() * dt * dt;
        const auto secondDifference = angles[step + 1] - 2.0f * angles[step] + angles[step - 1];

        EXPECT_NEAR(secondDifference, expected, 0.01f * std::abs(expected) + 1e-9f) << "step " << step;
    }
}
