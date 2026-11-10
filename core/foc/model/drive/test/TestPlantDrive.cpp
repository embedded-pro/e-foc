#include "core/foc/model/drive/PlantDrive.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    constexpr uint32_t baseHz = 20000;
    constexpr uint32_t ticksPerMillisecond = baseHz / 1000;

    const foc::PhasePwmDutyCycles neutral{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) };
    const foc::PhasePwmDutyCycles driving{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(45), hal::DutyCycle::FromPercent(45) };

    sil::SilPlantConfig Description()
    {
        const auto& motor = foc::M_2310P_LN_04K::parameters;

        sil::SilPlantConfig plant{};
        plant.statorResistanceOhm = motor.R.Value();
        plant.dAxisInductanceHenry = motor.Ld.Value();
        plant.qAxisInductanceHenry = motor.Lq.Value();
        plant.fluxLinkageWeber = motor.psi_f.Value();
        plant.rotorInertiaKgM2 = motor.J.Value();
        plant.viscousDampingNmSPerRad = motor.B.Value();
        plant.maxSupportedCurrentAmpere = motor.maxSupportedCurrent.Value();
        plant.polePairs = motor.p;
        plant.powerSupplyVoltageVolts = 24.0f;
        plant.baseFrequencyHz = baseHz;
        plant.ambientCelsius = 25.0f;
        plant.thermalResistance = 2.0f;
        plant.thermalCapacitance = 25.0f;
        plant.copperTempCoeff = 0.00393f;
        plant.supplyVoltageScale = 1.0f;
        return plant;
    }

    const sil::PlantDriveDefaults defaults{
        .motor = foc::M_2310P_LN_04K::parameters,
        .supplyVoltage = foc::Volts{ 48.0f },
        .baseFrequency = hal::Hertz{ 10000 },
    };

    void RunTicks(sil::PlantDrive& drive, uint32_t ticks)
    {
        for (uint32_t i = 0; i != ticks; ++i)
            drive.Tick();
    }

    float CurrentMagnitude(sil::PlantDrive& drive)
    {
        const auto dq = drive.Model().Electrical().LastDqCurrents();
        return std::abs(dq.d) + std::abs(dq.q);
    }
}

TEST(TestPlantDrive, without_a_description_the_defaults_set_the_rate_and_the_supply)
{
    sil::PlantDrive drive{ std::nullopt, defaults };

    EXPECT_EQ(drive.BaseFrequency(), hal::Hertz{ 10000 });
    EXPECT_NEAR(drive.Model().Supply().Voltage().Value(), 48.0f, 1e-6f);
    EXPECT_EQ(drive.DutyUpdateDelayPeriods(), 0u);
}

TEST(TestPlantDrive, the_description_sets_the_rate_and_the_supply)
{
    sil::PlantDrive drive{ Description(), defaults };

    EXPECT_EQ(drive.BaseFrequency(), hal::Hertz{ baseHz });
    EXPECT_NEAR(drive.Model().Supply().Voltage().Value(), 24.0f, 1e-6f);
}

TEST(TestPlantDrive, each_tick_advances_the_count)
{
    sil::PlantDrive drive{ Description(), defaults };

    RunTicks(drive, 3);

    EXPECT_EQ(drive.TickCount(), 3u);
}

TEST(TestPlantDrive, without_a_duty_delay_the_output_reaches_the_winding_in_the_same_period)
{
    sil::PlantDrive drive{ Description(), defaults };
    drive.Start();

    drive.Output(driving);
    drive.Tick();

    EXPECT_GT(CurrentMagnitude(drive), 1e-4f);
}

TEST(TestPlantDrive, a_duty_delay_holds_the_output_back_one_period)
{
    auto plant = Description();
    plant.dutyDelayPeriods = 1;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();

    drive.Output(driving);
    drive.Tick();
    const auto heldBack = CurrentMagnitude(drive);
    drive.Tick();

    EXPECT_EQ(drive.DutyUpdateDelayPeriods(), 1u);
    EXPECT_NEAR(heldBack, 0.0f, 1e-6f);
    EXPECT_GT(CurrentMagnitude(drive), 1e-4f);
}

TEST(TestPlantDrive, the_torque_step_fires_after_its_delay_from_start)
{
    auto plant = Description();
    plant.torqueStepNm = 0.05f;
    plant.torqueStepDelayMs = 2;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);

    RunTicks(drive, 2 * ticksPerMillisecond);
    EXPECT_NEAR(drive.Model().Mechanics().ExternalTorque().Value(), 0.0f, 1e-9f);

    drive.Tick();
    EXPECT_NEAR(drive.Model().Mechanics().ExternalTorque().Value(), 0.05f, 1e-9f);
}

TEST(TestPlantDrive, stop_removes_the_external_torque_and_restores_the_described_faults)
{
    auto plant = Description();
    plant.torqueStepNm = 0.05f;
    plant.torqueStepDelayMs = 1;
    plant.cableDisconnectDelayMs = 1;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(driving);
    RunTicks(drive, 2 * ticksPerMillisecond);
    ASSERT_NEAR(drive.Model().Mechanics().ExternalTorque().Value(), 0.05f, 1e-9f);

    drive.Stop();
    drive.Start();
    drive.Output(driving);
    drive.Tick();

    EXPECT_NEAR(drive.Model().Mechanics().ExternalTorque().Value(), 0.0f, 1e-9f);
    EXPECT_GT(CurrentMagnitude(drive), 1e-4f);
}

TEST(TestPlantDrive, a_cable_disconnect_opens_every_phase)
{
    auto plant = Description();
    plant.cableDisconnectDelayMs = 1;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(driving);

    RunTicks(drive, 3 * ticksPerMillisecond);

    EXPECT_NEAR(CurrentMagnitude(drive), 0.0f, 1e-6f);
}

TEST(TestPlantDrive, an_encoder_freeze_holds_the_reported_angle)
{
    auto plant = Description();
    plant.encoderFreezeDelayCentiseconds = 1;
    plant.torqueStepNm = -0.05f;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);

    RunTicks(drive, 10 * ticksPerMillisecond + 1);
    const auto frozen = drive.Model().Encoder().Read();
    RunTicks(drive, 10 * ticksPerMillisecond);

    EXPECT_NEAR(drive.Model().Encoder().Read().Value(), frozen.Value(), 1e-9f);
    EXPECT_GT(std::abs(drive.Model().Mechanics().Angle().Value()), 1e-3f);
}

TEST(TestPlantDrive, the_index_is_latched_until_rearmed)
{
    auto plant = Description();
    plant.encoderCountsPerRevolution = 4000;
    plant.encoderIndexRadians = 0.01f;
    plant.encoderIndexWidthCounts = 4;
    plant.torqueStepNm = -0.05f;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);
    drive.ArmIndex();

    ASSERT_TRUE(drive.Model().Encoder().HasIndex());
    for (uint32_t i = 0; i != 200 * ticksPerMillisecond && !drive.IndexSeen(); ++i)
        drive.Tick();
    EXPECT_TRUE(drive.IndexSeen());

    drive.ArmIndex();
    EXPECT_FALSE(drive.IndexSeen());
}

TEST(TestPlantDrive, the_response_is_recorded_at_the_described_rate)
{
    auto plant = Description();
    plant.responseSampleRateHz = 1000;
    plant.responseMaxSamples = 8;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);

    RunTicks(drive, 3 * ticksPerMillisecond);

    const auto began = drive.PopRecord();
    ASSERT_TRUE(began.has_value());
    EXPECT_EQ(began->kind, foc::PlantResponseKind::began);

    uint32_t samples = 0;
    while (const auto record = drive.PopRecord())
        samples += record->kind == foc::PlantResponseKind::sample ? 1 : 0;
    EXPECT_EQ(samples, 3u);
}

TEST(TestPlantDrive, offsets_and_readable_shunts_shape_the_currents_the_controller_reads)
{
    auto plant = Description();
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(driving);
    drive.Tick();

    const auto sensed = drive.Model().CurrentSensor().Last();
    const auto read = drive.ShuntCurrents();

    EXPECT_NEAR(read.a.Value(), sensed.a.Value(), 1e-6f);
    EXPECT_NEAR(read.b.Value(), sensed.b.Value(), 1e-6f);
    EXPECT_NEAR(read.c.Value(), sensed.c.Value(), 1e-6f);
}

TEST(TestPlantDrive, a_supply_step_scales_the_open_circuit_voltage)
{
    auto plant = Description();
    plant.supplyStepScale = 0.5f;
    plant.supplyStepDelayMs = 1;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);

    RunTicks(drive, ticksPerMillisecond);
    EXPECT_NEAR(drive.Model().Supply().Voltage().Value(), 24.0f, 1e-3f);

    RunTicks(drive, 1);
    EXPECT_NEAR(drive.Model().Supply().Voltage().Value(), 12.0f, 1e-3f);
}

TEST(TestPlantDrive, a_shaft_grip_turns_the_rotor_then_holds_it)
{
    auto plant = Description();
    plant.shaftTurnRadians = 0.5f;
    plant.shaftGripDelayMs = 1;
    plant.shaftTurnMs = 10;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);

    RunTicks(drive, 5 * ticksPerMillisecond);
    EXPECT_NEAR(drive.Model().Mechanics().Speed().Value(), 50.0f, 1e-3f);

    RunTicks(drive, 10 * ticksPerMillisecond);
    EXPECT_NEAR(drive.Model().Mechanics().Speed().Value(), 0.0f, 1e-6f);
    EXPECT_NEAR(drive.Model().Mechanics().Angle().Value(), 0.5f, 0.01f);
}

TEST(TestPlantDrive, the_dead_time_loss_comes_from_the_description)
{
    auto plant = Description();
    plant.deadTimeCompensationNanoseconds = 149.6f;
    plant.deadTimeCompensationKneeAmpere = 0.3f;
    sil::PlantDrive described{ plant, defaults };
    sil::PlantDrive undescribed{ std::nullopt, defaults };

    EXPECT_EQ(described.InverterDeadTimeLoss().effectiveDeadTime, std::chrono::nanoseconds{ 150 });
    EXPECT_NEAR(described.InverterDeadTimeLoss().kneeCurrent.Value(), 0.3f, 1e-6f);
    EXPECT_EQ(undescribed.InverterDeadTimeLoss().effectiveDeadTime, std::chrono::nanoseconds{ 0 });
}

TEST(TestPlantDrive, a_longer_sample_and_hold_delays_what_the_sensor_reports)
{
    auto plant = Description();
    plant.adcClockMegahertz = 16.0f;
    plant.adcConversionClocks = 12;
    plant.adcAveragedConversions = 1;
    plant.currentFullScaleAmpere = 8.25f;
    plant.adcResolutionBits = 12;
    sil::PlantDrive shortHold{ plant, defaults };
    sil::PlantDrive longHold{ plant, defaults };
    longHold.SampleAndHoldClocks(256);

    for (auto* drive : { &shortHold, &longHold })
    {
        drive->Start();
        drive->Output(driving);
        RunTicks(*drive, 5);
    }

    EXPECT_NE(shortHold.Model().CurrentSensor().Last().a.Value(), longHold.Model().CurrentSensor().Last().a.Value());
}

TEST(TestPlantDrive, the_applied_duties_are_those_the_legs_ran_in_the_last_period)
{
    sil::PlantDrive drive{ Description(), defaults };
    drive.Start();

    drive.Output(driving);
    drive.Tick();

    EXPECT_EQ(drive.AppliedDuties().a, driving.a);
    EXPECT_EQ(drive.AppliedDuties().b, driving.b);
}

TEST(TestPlantDrive, with_a_duty_delay_the_applied_duties_lag_one_period)
{
    auto plant = Description();
    plant.dutyDelayPeriods = 1;
    sil::PlantDrive drive{ plant, defaults };
    drive.Start();
    drive.Output(neutral);
    drive.Tick();

    drive.Output(driving);
    drive.Tick();
    EXPECT_EQ(drive.AppliedDuties().a, neutral.a);

    drive.Tick();
    EXPECT_EQ(drive.AppliedDuties().a, driving.a);
}
