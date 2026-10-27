#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <chrono>
#include <gtest/gtest.h>

namespace
{
    using mil::Quantity;
    using namespace std::chrono_literals;

    class TestIdentificationRobustness
        : public ::testing::Test
    {
    protected:
        mil::Scenario MechanicalScenario(const char* motor)
        {
            mil::Scenario scenario;
            scenario.Plant(motor);
            return scenario;
        }

        void CalibrateFromAWrongMechanicalSeed(mil::Scenario& scenario)
        {
            scenario.Calibrated().StoredCalibrationOffBy("inertia", 2.0f).StoredCalibrationOffBy("friction", 0.5f).BootsIn(state_machine::ControlMode::speed);
        }

        void Excite(mil::ModelInTheLoop& mil)
        {
            ASSERT_TRUE(mil.Align());
            ASSERT_TRUE(mil.Enable());
            ASSERT_TRUE(mil.AlternateSpeed(26.0f, 52.0f, 250ms, 6000ms));
        }
    };
}

TEST_F(TestIdentificationRobustness, the_full_calibration_identifies_the_bench_motor_with_dry_friction)
{
    for (const float coulomb : { 0.0f, 0.0005f, 0.002f })
    {
        SCOPED_TRACE(coulomb);
        auto scenario = MechanicalScenario("bench");
        scenario.plant.coulombFrictionNm = coulomb;
        CalibrateFromAWrongMechanicalSeed(scenario);

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Calibrate());
        mil::ExpectIdentified(mil, Quantity::resistance, 5.0f);
        mil::ExpectPolePairsMatch(mil);
        mil::ExpectIdentified(mil, Quantity::inertia, 20.0f);
    }
}

TEST_F(TestIdentificationRobustness, the_full_calibration_reads_the_viscous_friction_through_a_once_per_turn_torque)
{
    auto scenario = MechanicalScenario("bench");
    scenario.plant.viscousDampingNmSPerRad = 0.000012f;
    scenario.plant.coulombFrictionNm = 0.0037f;
    scenario.plant.coggingTorqueNm = 0.001f;
    scenario.plant.coggingCyclesPerRevolution = 1;
    CalibrateFromAWrongMechanicalSeed(scenario);

    mil::ModelInTheLoop mil{ scenario };
    ASSERT_TRUE(mil.Calibrate());
    mil::ExpectIdentified(mil, Quantity::friction, 10.0f);
    mil::ExpectIdentified(mil, Quantity::inertia, 5.0f);
}

TEST_F(TestIdentificationRobustness, the_electrical_identification_reads_through_a_non_ideal_power_stage)
{
    static constexpr std::array<std::pair<float, float>, 3> rows{ { { 0.0f, 5.0f }, { 50.0f, 5.0f }, { 270.0f, 15.0f } } };

    for (const auto& [deadTimeNs, resistancePercent] : rows)
    {
        SCOPED_TRACE(deadTimeNs);
        mil::Scenario scenario;
        scenario.Plant("bench").BootsIn(state_machine::ControlMode::torque);
        scenario.plant.deadTimeNanoseconds = deadTimeNs;
        scenario.plant.switchNodeCapacitancePicofarad = 300.0f;
        scenario.plant.switchOnResistanceOhm = 0.0125f;
        scenario.plant.diodeForwardVolts = 0.8f;

        mil::ModelInTheLoop mil{ scenario };
        const auto identified = mil.IdentifyElectrical();
        ASSERT_TRUE(identified.has_value());
        mil::ExpectWithin(scenario, "identified", Quantity::resistance, identified->resistanceOhm, resistancePercent);
        EXPECT_EQ(identified->polePairs, scenario.plant.polePairs);
    }
}

TEST_F(TestIdentificationRobustness, uncompensated_the_benchs_dead_time_reads_the_calibration_resistance_high)
{
    mil::Scenario scenario;
    scenario.Plant("bench").BootsIn(state_machine::ControlMode::torque);
    sil::DescribeBenchPowerStage(scenario.plant);

    mil::ModelInTheLoop mil{ scenario };
    const auto identified = mil.IdentifyElectrical();
    ASSERT_TRUE(identified.has_value());
    mil::ExpectAbove(scenario, "identified", Quantity::resistance, identified->resistanceOhm, 3.5f);
}

TEST_F(TestIdentificationRobustness, uncompensated_the_benchs_dead_time_reads_the_online_resistance_high)
{
    mil::Scenario scenario;
    scenario.Plant("bench");
    sil::DescribeBenchPowerStage(scenario.plant);
    scenario.Recorded(100, 2000).Calibrated().StoredCalibrationOffBy("resistance", 1.3f).BootsIn(state_machine::ControlMode::speed);

    mil::ModelInTheLoop mil{ scenario };
    Excite(mil);
    const auto estimates = mil.OnlineEstimates();
    ASSERT_TRUE(estimates.has_value());
    mil::ExpectAbove(scenario, "online", Quantity::resistance, mil::Online(*estimates, Quantity::resistance), 12.0f);
}

TEST_F(TestIdentificationRobustness, compensating_the_benchs_dead_time_keeps_the_online_resistance_on_the_winding)
{
    mil::Scenario scenario;
    scenario.Plant("bench");
    sil::DescribeBenchPowerStage(scenario.plant);
    sil::DescribeBenchDeadTimeCompensation(scenario.plant);
    scenario.Recorded(100, 2000).Calibrated().StoredCalibrationOffBy("resistance", 1.3f).BootsIn(state_machine::ControlMode::speed);

    mil::ModelInTheLoop mil{ scenario };
    Excite(mil);
    mil::ExpectOnline(mil, Quantity::resistance, 5.0f);
}

TEST_F(TestIdentificationRobustness, the_electrical_identification_reads_through_the_benchs_current_sense_chain)
{
    mil::Scenario scenario;
    scenario.Plant("bench").BootsIn(state_machine::ControlMode::torque);
    sil::DescribeBenchCurrentSense(scenario.plant);

    mil::ModelInTheLoop mil{ scenario };
    const auto identified = mil.IdentifyElectrical();
    ASSERT_TRUE(identified.has_value());
    mil::ExpectWithin(scenario, "identified", Quantity::resistance, identified->resistanceOhm, 5.0f);
    EXPECT_EQ(identified->polePairs, scenario.plant.polePairs);
}

TEST_F(TestIdentificationRobustness, the_inertia_is_unbiased_on_a_quantised_encoder)
{
    static constexpr std::array<std::pair<const char*, uint32_t>, 5> rows{ {
        { "bench", 4000 },
        { "bench", 20000 },
        { "bench", 1000 },
        { "teknic", 4000 },
        { "anaheim", 4000 },
    } };

    for (const auto& [motor, counts] : rows)
    {
        SCOPED_TRACE(motor);
        SCOPED_TRACE(counts);
        auto scenario = MechanicalScenario(motor);
        scenario.plant.encoderCountsPerRevolution = counts;
        CalibrateFromAWrongMechanicalSeed(scenario);

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Calibrate());
        mil::ExpectIdentified(mil, Quantity::inertia, 1.0f);
    }
}

TEST_F(TestIdentificationRobustness, the_calibration_completes_with_encoder_noise)
{
    static constexpr std::array<std::tuple<const char*, float, float>, 4> rows{ {
        { "bench", 0.00005f, 20.0f },
        { "bench", 0.0002f, 20.0f },
        { "teknic", 0.0002f, 10.0f },
        { "anaheim", 0.0002f, 10.0f },
    } };

    for (const auto& [motor, sigma, inertiaPercent] : rows)
    {
        SCOPED_TRACE(motor);
        SCOPED_TRACE(sigma);
        auto scenario = MechanicalScenario(motor);
        scenario.plant.encoderSigmaRadians = sigma;
        CalibrateFromAWrongMechanicalSeed(scenario);

        mil::ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Calibrate());
        mil::ExpectIdentified(mil, Quantity::inertia, inertiaPercent);
    }
}
