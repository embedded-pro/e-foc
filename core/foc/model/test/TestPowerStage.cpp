#include "core/foc/model/PowerStage.hpp"
#include <gtest/gtest.h>

namespace
{
    constexpr float busVoltage{ 24.0f };
    constexpr float period{ 50e-6f };
    constexpr float deadTime{ 500e-9f };
    constexpr float capacitance{ 1e-9f };
    constexpr float diode{ 0.8f };

    class TestPowerStage
        : public ::testing::Test
    {
    protected:
        foc::PowerStageConfig stage{ deadTime, capacitance, 0.0f, diode, 0.0f };
    };
}

TEST_F(TestPowerStage, an_ideal_stage_delivers_the_commanded_duty)
{
    const foc::PowerStageConfig ideal{};

    for (const auto duty : { 0.0f, 0.1f, 0.5f, 0.93f, 1.0f })
        EXPECT_NEAR(foc::PowerStageLegVoltage(ideal, duty, 3.0f, busVoltage, period), duty * busVoltage, 1e-5f);
}

TEST_F(TestPowerStage, without_current_the_node_stays_where_the_last_switch_left_it)
{
    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, 0.0f, busVoltage, period), 0.4f * busVoltage, 1e-5f);
}

TEST_F(TestPowerStage, a_large_current_loses_the_whole_dead_time_against_its_sign)
{
    const auto loss = (busVoltage + 2.0f * diode) * deadTime / period;

    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, 5.0f, busVoltage, period), 0.4f * busVoltage - loss, 2e-3f);
    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, -5.0f, busVoltage, period), 0.4f * busVoltage + loss, 2e-3f);
}

TEST_F(TestPowerStage, a_current_too_small_to_reach_either_diode_loses_voltage_in_proportion_to_it)
{
    const auto current = 1e-3f;
    const auto loss = current * deadTime * deadTime / (capacitance * period);

    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, current, busVoltage, period), 0.4f * busVoltage - loss, 1e-5f);
    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, -current, busVoltage, period), 0.4f * busVoltage + loss, 1e-5f);
}

TEST_F(TestPowerStage, a_small_current_swings_the_bus_edge_partway_and_clamps_the_other_at_the_diode)
{
    const auto current = 0.01f;
    const auto busEdge = current * deadTime * deadTime / (2.0f * capacitance);
    const auto diodeEdge = diode * deadTime - diode * diode * capacitance / (2.0f * current);
    const auto loss = (busEdge + diodeEdge) / period;

    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, current, busVoltage, period), 0.4f * busVoltage - loss, 1e-5f);
    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.4f, -current, busVoltage, period), 0.4f * busVoltage + loss, 1e-5f);
}

TEST_F(TestPowerStage, the_conducting_switch_drops_the_phase_current_times_its_resistance)
{
    const foc::PowerStageConfig resistive{ 0.0f, 0.0f, 0.1f, 0.0f, 0.0f };

    EXPECT_NEAR(foc::PowerStageLegVoltage(resistive, 0.3f, 2.0f, busVoltage, period), 0.3f * busVoltage - 0.2f, 1e-5f);
}

TEST_F(TestPowerStage, a_high_pulse_shorter_than_the_minimum_leaves_the_leg_floating_from_low)
{
    stage.minimumPulseSeconds = 1e-6f;
    const auto duty = 0.02f;

    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, duty, 5.0f, busVoltage, period), -diode * (duty * period + deadTime) / period, 1e-3f);
}

TEST_F(TestPowerStage, a_duty_at_either_rail_switches_nothing)
{
    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 0.0f, -2.0f, busVoltage, period), 0.0f, 1e-6f);
    EXPECT_NEAR(foc::PowerStageLegVoltage(stage, 1.0f, 2.0f, busVoltage, period), busVoltage, 1e-6f);
}

TEST_F(TestPowerStage, the_low_side_conducts_for_the_low_pulse_around_the_period_centre)
{
    const auto window = foc::LowSideConduction({}, 0.6f, 1.0f, period);

    EXPECT_NEAR(window.start, -10e-6f, 1e-10f);
    EXPECT_NEAR(window.end, 10e-6f, 1e-10f);
}

TEST_F(TestPowerStage, a_current_out_of_the_leg_keeps_the_low_side_diode_conducting_through_the_dead_time)
{
    const auto half = 0.5f * (0.4f * period - deadTime);

    EXPECT_NEAR(foc::LowSideConduction(stage, 0.6f, 1.0f, period).end, half + deadTime, 1e-10f);
    EXPECT_NEAR(foc::LowSideConduction(stage, 0.6f, -1.0f, period).end, half, 1e-10f);
}

TEST_F(TestPowerStage, a_low_pulse_shorter_than_the_minimum_leaves_only_the_diode_to_conduct)
{
    stage.minimumPulseSeconds = 1e-6f;
    const auto duty = 0.98f;

    EXPECT_NEAR(foc::LowSideConduction(stage, duty, 1.0f, period).end, 0.5f * ((1.0f - duty) * period + deadTime), 1e-10f);
    EXPECT_NEAR(foc::LowSideConduction(stage, duty, -1.0f, period).end, 0.0f, 1e-10f);
}

TEST_F(TestPowerStage, the_low_side_conducts_all_period_at_zero_duty_and_never_at_full_duty)
{
    EXPECT_NEAR(foc::LowSideConduction(stage, 0.0f, -1.0f, period).end, period, 1e-10f);
    EXPECT_NEAR(foc::LowSideConduction(stage, 1.0f, 1.0f, period).end, 0.0f, 1e-10f);
}
