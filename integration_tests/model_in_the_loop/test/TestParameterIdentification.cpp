#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <chrono>
#include <gtest/gtest.h>

namespace
{
    using mil::Quantity;
    using namespace std::chrono_literals;

    constexpr std::array<const char*, 2> referenceMotors{ "teknic", "anaheim" };

    class TestParameterIdentification
        : public ::testing::Test
    {
    protected:
        void ExpectElectricalIdentification(const mil::Scenario& scenario, float resistancePercent, float inductancePercent)
        {
            SCOPED_TRACE(scenario.plantName);
            mil::ModelInTheLoop mil{ scenario };

            const auto identified = mil.IdentifyElectrical();
            ASSERT_TRUE(identified.has_value());
            mil::ExpectWithin(scenario, "identified", Quantity::resistance, identified->resistanceOhm, resistancePercent);
            mil::ExpectWithin(scenario, "identified", Quantity::inductance, identified->inductanceMilliHenry, inductancePercent);
            EXPECT_EQ(identified->polePairs, scenario.plant.polePairs);
        }

        mil::Scenario OnlineScenario(const char* motor)
        {
            mil::Scenario scenario;
            scenario.Plant(motor).Recorded(100, 2000);
            return scenario;
        }

        void Excite(mil::ModelInTheLoop& mil)
        {
            ASSERT_TRUE(mil.Align());
            ASSERT_TRUE(mil.Enable());
            ASSERT_TRUE(mil.AlternateSpeed(26.0f, 52.0f, 250ms, 6000ms));
        }
    };
}

TEST_F(TestParameterIdentification, electrical_identification_reports_the_plants_resistance_inductance_and_pole_pairs)
{
    for (const auto* motor : referenceMotors)
        ExpectElectricalIdentification(mil::Scenario{}.Plant(motor).BootsIn(state_machine::ControlMode::torque), 5.0f, 20.0f);
}

TEST_F(TestParameterIdentification, electrical_identification_holds_up_against_measurement_noise)
{
    for (const auto* motor : referenceMotors)
    {
        mil::Scenario scenario;
        scenario.Plant(motor).BootsIn(state_machine::ControlMode::torque);
        scenario.plant.noiseSigmaAmpere = 0.05f;
        scenario.plant.noiseBiasAmpereA = 0.02f;
        scenario.plant.encoderSigmaRadians = 0.0002f;
        ExpectElectricalIdentification(scenario, 10.0f, 25.0f);
    }
}

TEST_F(TestParameterIdentification, the_full_calibration_measures_the_current_sensors_offsets)
{
    for (const auto* motor : { "teknic", "anaheim", "bench" })
    {
        SCOPED_TRACE(motor);
        mil::Scenario scenario;
        scenario.Plant(motor).BootsIn(state_machine::ControlMode::torque);
        scenario.plant.noiseSigmaAmpere = 0.035f;
        scenario.plant.noiseBiasAmpereA = 0.027f;
        scenario.plant.noiseBiasAmpereB = -0.018f;
        scenario.plant.noiseBiasAmpereC = 0.054f;

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Calibrate());
        mil::ExpectOffsetsWithin(scenario, mil.CalibrationRecord(), 5.0f);
        mil::ExpectIdentified(mil, Quantity::resistance, 5.0f);
        mil::ExpectPolePairsMatch(mil);
    }
}

TEST_F(TestParameterIdentification, the_full_calibration_identifies_the_mechanical_parameters)
{
    for (const auto* motor : referenceMotors)
    {
        SCOPED_TRACE(motor);
        mil::Scenario scenario;
        scenario.Plant(motor).Calibrated().StoredCalibrationOffBy("resistance", 1.3f).StoredCalibrationOffBy("inductance", 0.7f).StoredCalibrationOffBy("inertia", 2.0f).StoredCalibrationOffBy("friction", 0.5f).BootsIn(state_machine::ControlMode::speed);

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Calibrate());
        mil::ExpectIdentified(mil, Quantity::resistance, 5.0f);
        mil::ExpectIdentified(mil, Quantity::inductance, 20.0f);
        mil::ExpectPolePairsMatch(mil);
        mil::ExpectIdentified(mil, Quantity::inertia, 5.0f);
        mil::ExpectIdentified(mil, Quantity::friction, 10.0f);
    }
}

TEST_F(TestParameterIdentification, the_online_estimators_converge_from_a_wrong_seed_under_excitation)
{
    for (const auto* motor : referenceMotors)
    {
        SCOPED_TRACE(motor);
        auto scenario = OnlineScenario(motor);
        scenario.Calibrated().StoredCalibrationOffBy("resistance", 1.3f).StoredCalibrationOffBy("inductance", 0.7f).StoredCalibrationOffBy("inertia", 2.0f).StoredCalibrationOffBy("friction", 0.5f).BootsIn(state_machine::ControlMode::speed);

        mil::ModelInTheLoop mil{ scenario };
        Excite(mil);
        mil::ExpectOnline(mil, Quantity::inertia, 2.0f);
        mil::ExpectOnline(mil, Quantity::friction, 5.0f);
        mil::ExpectOnline(mil, Quantity::inductance, 10.0f);
        mil::ExpectOnline(mil, Quantity::resistance, 10.0f);
    }
}

TEST_F(TestParameterIdentification, the_online_estimators_converge_through_the_bench_motors_encoder_and_current_noise)
{
    auto scenario = OnlineScenario("bench");
    scenario.Calibrated().StoredCalibrationOffBy("inductance", 0.7f).StoredCalibrationOffBy("inertia", 2.0f).StoredCalibrationOffBy("friction", 0.5f).BootsIn(state_machine::ControlMode::speed);

    mil::ModelInTheLoop mil{ scenario };
    Excite(mil);
    mil::ExpectOnline(mil, Quantity::inertia, 2.0f);
    mil::ExpectOnline(mil, Quantity::friction, 5.0f);
    mil::ExpectOnline(mil, Quantity::inductance, 10.0f);
}

TEST_F(TestParameterIdentification, a_stop_through_zero_leaves_the_online_friction_where_the_speed_levels_put_it)
{
    for (const auto algorithm : { foc::SpeedAlgorithm::pid, foc::SpeedAlgorithm::lqi, foc::SpeedAlgorithm::adrc })
    {
        SCOPED_TRACE(static_cast<int>(algorithm));
        auto scenario = OnlineScenario("bench");
        sil::DescribeBenchFrictionAndCogging(scenario.plant);
        scenario.plant.viscousDampingNmSPerRad = 0.00001f;
        scenario.Calibrated().BootsIn(state_machine::ControlMode::speed).SpeedLoop(algorithm);

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Align());
        ASSERT_TRUE(mil.Enable());
        ASSERT_TRUE(mil.HoldSpeed(30.0f, 2500ms));
        ASSERT_TRUE(mil.HoldSpeed(60.0f, 2500ms));
        ASSERT_TRUE(mil.HoldSpeed(-30.0f, 2500ms));
        ASSERT_TRUE(mil.HoldSpeed(0.0f, 2500ms));
        mil::ExpectOnline(mil, Quantity::friction, 10.0f);
    }
}

TEST_F(TestParameterIdentification, the_online_electrical_estimates_hold_under_every_current_law_when_each_duty_acts_a_period_late)
{
    for (const auto algorithm : { foc::CurrentAlgorithm::pid, foc::CurrentAlgorithm::decoupledPid, foc::CurrentAlgorithm::deadbeat, foc::CurrentAlgorithm::slidingMode })
    {
        SCOPED_TRACE(static_cast<int>(algorithm));
        auto scenario = OnlineScenario("bench");
        sil::DescribeBoardPwmUpdate(scenario.plant);
        scenario.Calibrated().StoredCalibrationOffBy("resistance", 1.3f).BootsIn(state_machine::ControlMode::speed).CurrentLoop(algorithm);

        mil::ModelInTheLoop mil{ scenario };
        Excite(mil);
        mil::ExpectOnline(mil, Quantity::resistance, 5.0f);
        mil::ExpectOnline(mil, Quantity::inductance, 10.0f);
    }
}

TEST_F(TestParameterIdentification, the_full_calibration_maps_a_cogging_a_quarter_of_the_benchs)
{
    mil::Scenario scenario;
    scenario.Plant("bench").BootsIn(state_machine::ControlMode::speed);
    scenario.plant.coulombFrictionNm = 0.0037f;
    scenario.plant.coggingTorqueNm = 0.001f;
    scenario.plant.coggingCyclesPerRevolution = 24;

    mil::ModelInTheLoop mil{ scenario };
    ASSERT_TRUE(mil.Calibrate());
    mil::ExpectIdentified(mil, Quantity::cogging, 30.0f);
}

TEST_F(TestParameterIdentification, the_full_calibration_stores_no_cogging_map_for_a_motor_without_cogging)
{
    mil::Scenario scenario;
    scenario.Plant("bench").BootsIn(state_machine::ControlMode::speed);
    scenario.plant.coulombFrictionNm = 0.0037f;

    mil::ModelInTheLoop mil{ scenario };
    ASSERT_TRUE(mil.Calibrate());
    EXPECT_EQ(mil::Identified(mil.CalibrationRecord(), Quantity::cogging), 0.0f);
    mil::ExpectIdentified(mil, Quantity::inertia, 5.0f);
}

TEST_F(TestParameterIdentification, a_constant_shaft_torque_does_not_bias_the_online_mechanical_estimates)
{
    for (const auto* motor : referenceMotors)
    {
        SCOPED_TRACE(motor);
        auto scenario = OnlineScenario(motor);
        scenario.TorqueStep(0.01f, 500).Calibrated().StoredCalibrationOffBy("inertia", 2.0f).StoredCalibrationOffBy("friction", 0.5f).BootsIn(state_machine::ControlMode::speed);

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Align());
        ASSERT_TRUE(mil.Enable());
        ASSERT_TRUE(mil.CaptureAfterEnable(700.0f));
        ASSERT_TRUE(mil.AlternateSpeed(26.0f, 52.0f, 250ms, 6000ms));
        mil::ExpectOnline(mil, Quantity::inertia, 2.0f);
        mil::ExpectOnline(mil, Quantity::friction, 15.0f);
    }
}

TEST_F(TestParameterIdentification, winding_heating_is_tracked_by_the_online_resistance_estimate)
{
    auto scenario = OnlineScenario("hot");
    scenario.Calibrated().BootsIn(state_machine::ControlMode::speed);

    mil::ModelInTheLoop mil{ scenario };
    Excite(mil);
    mil::ExpectOnline(mil, Quantity::resistance, 10.0f, 90.0f);
}
