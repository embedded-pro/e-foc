#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "motor_parameters/Jk42bls01X038ed.hpp"
#include <gtest/gtest.h>

namespace
{
    const foc::PhasePwmDutyCycles kNeutralDuty{
        hal::Percent{ 60 }, hal::Percent{ 50 }, hal::Percent{ 40 }
    };

    foc::ThreePhaseMotorModel MakeModel(std::optional<std::size_t> limit = std::nullopt)
    {
        return foc::ThreePhaseMotorModel{
            foc::JK42BLS01_X038ED::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 20000 },
            limit
        };
    }

    class RecordingObserver
        : public foc::ThreePhaseMotorModelObserver
    {
    public:
        explicit RecordingObserver(foc::ThreePhaseMotorModel& model)
            : ThreePhaseMotorModelObserver(model)
        {}

        void Started() override { ++startCount; }
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents, foc::Radians, foc::RadiansPerSecond) override { ++phaseCurrentCount; }
        void StatorVoltages(foc::ThreePhase, foc::TwoPhase) override { ++statorVoltageCount; }
        void Finished() override { ++finishCount; }

        int startCount{};
        int phaseCurrentCount{};
        int statorVoltageCount{};
        int finishCount{};
    };

    class MotorModelTest
        : public ::testing::Test
    {
    protected:
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;
        foc::ThreePhaseMotorModel model{ MakeModel() };
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

TEST_F(MotorModelTest, iteration_limit_notifies_finished_and_stops_callback)
{
    foc::ThreePhaseMotorModel limitedModel{ MakeModel(3) };
    int callbackCount = 0;
    limitedModel.PhaseCurrentsReady(hal::Hertz{ 20000 }, [&callbackCount](foc::PhaseCurrents)
        {
            ++callbackCount;
        });

    RecordingObserver obs{ limitedModel };
    limitedModel.Start();

    limitedModel.StepForTest(kNeutralDuty);
    limitedModel.StepForTest(kNeutralDuty);
    limitedModel.StepForTest(kNeutralDuty);

    EXPECT_EQ(obs.finishCount, 1);
    EXPECT_EQ(callbackCount, 2);
}

TEST_F(MotorModelTest, set_and_read_angle_round_trips)
{
    model.Set(foc::Radians{ 1.5f });
    EXPECT_FLOAT_EQ(model.Read().Value(), 1.5f);
}

TEST_F(MotorModelTest, set_zero_resets_angle)
{
    model.Set(foc::Radians{ 2.0f });
    model.SetZero();
    EXPECT_FLOAT_EQ(model.Read().Value(), 0.0f);
}

TEST_F(MotorModelTest, set_load_does_not_crash)
{
    model.SetLoad(foc::NewtonMeter{ 0.1f });
    model.StepForTest(kNeutralDuty);
}

TEST_F(MotorModelTest, effective_inductance_d_at_ambient_equals_ld)
{
    const float expected = foc::JK42BLS01_X038ED::parameters.Ld.Value();
    EXPECT_FLOAT_EQ(model.EffectiveInductanceD().Value(), expected);
}

TEST_F(MotorModelTest, effective_inductance_q_at_ambient_equals_lq)
{
    const float expected = foc::JK42BLS01_X038ED::parameters.Lq.Value();
    EXPECT_FLOAT_EQ(model.EffectiveInductanceQ().Value(), expected);
}

TEST_F(MotorModelTest, self_drive_fires_phase_currents_via_event_dispatcher)
{
    int callbackCount = 0;
    model.PhaseCurrentsReady(hal::Hertz{ 20000 }, [this, &callbackCount](foc::PhaseCurrents)
        {
            ++callbackCount;
            model.Stop();
        });

    model.ThreePhasePwmOutput(kNeutralDuty);
    eventDispatcher.ExecuteAllActions();

    EXPECT_EQ(callbackCount, 1);
}

TEST_F(MotorModelTest, stop_prevents_further_self_drive_callbacks)
{
    int callbackCount = 0;
    model.PhaseCurrentsReady(hal::Hertz{ 20000 }, [&callbackCount](foc::PhaseCurrents)
        {
            ++callbackCount;
        });

    model.ThreePhasePwmOutput(kNeutralDuty);
    model.Stop();
    eventDispatcher.ExecuteAllActions();

    EXPECT_EQ(callbackCount, 0);
}

TEST_F(MotorModelTest, thermal_config_applied_changes_effective_resistance)
{
    foc::ThreePhaseMotorModel::ThermalConfig cfg{};
    cfg.copperTempCoeff = 0.00393f;
    model.SetThermalConfig(cfg);
    model.SetWindingTemperatureForTest(125.0f);

    const float r0 = foc::JK42BLS01_X038ED::parameters.R.Value();
    EXPECT_GT(model.EffectiveResistance().Value(), r0);
}

TEST_F(MotorModelTest, observer_finished_not_called_without_iteration_limit)
{
    RecordingObserver obs{ model };
    for (int i = 0; i < 100; ++i)
        model.StepForTest(kNeutralDuty);
    EXPECT_EQ(obs.finishCount, 0);
}
