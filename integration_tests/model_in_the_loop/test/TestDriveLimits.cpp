#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>
#include <variant>

namespace
{
    constexpr uint32_t recordRateHz{ 1000 };
    constexpr uint32_t recordSamples{ 2000 };

    mil::Scenario TorqueModeWithOverspeedLimit(float limit)
    {
        auto scenario = mil::Scenario{}.Recorded(recordRateHz, recordSamples).Calibrated().BootsIn(state_machine::ControlMode::torque);
        scenario.nvm.config.limits.overspeedLimit = limit;
        return scenario;
    }

    float PeakSpeed(const integration::response::PlantTrace& trace)
    {
        float peak{ 0.0f };
        for (const auto& sample : trace.Samples())
            peak = std::max(peak, std::abs(sample.omegaMech));
        return peak;
    }

    float FinalSpeed(const integration::response::PlantTrace& trace)
    {
        return trace.Samples().empty() ? 0.0f : trace.Samples().back().omegaMech;
    }
}

TEST(TestDriveLimits, a_torque_command_without_load_settles_in_the_last_tenth_of_the_overspeed_limit)
{
    constexpr float limit{ 100.0f };
    mil::ModelInTheLoop mil{ TorqueModeWithOverspeedLimit(limit) };

    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.SetTorque(1.0f));
    ASSERT_TRUE(mil.CaptureAfterEnable(1500.0f));

    const auto peak = PeakSpeed(mil.Trace());
    const auto final = FinalSpeed(mil.Trace());
    std::fprintf(stderr, "[METRIC] mil limits torque_speed_limit limit=%.1f peak=%.3f final=%.3f\n", limit, peak, final);

    EXPECT_TRUE(mil.IsRunning());
    EXPECT_LE(peak, 1.02f * limit);
    EXPECT_LE(final, limit);
    EXPECT_GE(final, 0.9f * limit);
    EXPECT_TRUE(mil.Disable());
}

TEST(TestDriveLimits, without_a_limit_low_enough_to_bind_the_same_command_runs_past_it)
{
    constexpr float limit{ 100.0f };
    mil::ModelInTheLoop mil{ TorqueModeWithOverspeedLimit(services::LimitSettings{}.overspeedLimit) };

    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.SetTorque(1.0f));
    ASSERT_TRUE(mil.CaptureAfterEnable(1500.0f));

    const auto peak = PeakSpeed(mil.Trace());
    std::fprintf(stderr, "[METRIC] mil limits torque_unlimited peak=%.3f\n", peak);

    EXPECT_GT(peak, 1.2f * limit);
    EXPECT_TRUE(mil.Disable());
}

namespace
{
    struct RampResponse
    {
        float reachMs;
        float overshoot;
    };

    std::optional<RampResponse> MeasureRamp(mil::ModelInTheLoop& mil, float target, float band)
    {
        const auto setpointTick = mil.SetpointTick();
        if (!setpointTick)
            return std::nullopt;

        std::optional<uint32_t> reachTick;
        float peak{ 0.0f };
        for (const auto& sample : mil.Trace().Samples())
        {
            if (sample.tick < *setpointTick)
                continue;
            peak = std::max(peak, sample.omegaMech);
            if (!reachTick && std::abs(sample.omegaMech - target) <= band)
                reachTick = sample.tick;
        }

        if (!reachTick)
            return std::nullopt;

        const auto ticksPerMs = static_cast<float>(mil.TicksFromMilliseconds(1.0f));
        return RampResponse{ static_cast<float>(*reachTick - *setpointTick) / ticksPerMs, peak - target };
    }
}

TEST(TestDriveLimits, every_speed_law_follows_the_acceleration_ramp_to_its_setpoint)
{
    constexpr float acceleration{ 100.0f };
    constexpr float target{ 20.0f };
    constexpr float band{ 0.5f };

    for (const auto& [algorithm, name] : { std::pair{ foc::SpeedAlgorithm::pid, "pid" }, std::pair{ foc::SpeedAlgorithm::lqi, "lqi" },
             std::pair{ foc::SpeedAlgorithm::adrc, "adrc" }, std::pair{ foc::SpeedAlgorithm::twoDof, "twodof" } })
    {
        SCOPED_TRACE(name);
        auto scenario = mil::Scenario{}.Recorded(recordRateHz, recordSamples).Calibrated().BootsIn(state_machine::ControlMode::speed).SpeedLoop(algorithm);
        scenario.nvm.config.limits.speedAcceleration = acceleration;
        mil::ModelInTheLoop mil{ scenario };

        ASSERT_TRUE(mil.Align());
        ASSERT_TRUE(mil.Enable());
        ASSERT_TRUE(mil.SetSpeed(target));
        ASSERT_TRUE(mil.CaptureAfterSetpoint(600.0f));

        const auto response = MeasureRamp(mil, target, band);
        ASSERT_TRUE(response.has_value());
        std::fprintf(stderr, "[METRIC] mil limits speed_ramp %s reach_ms=%.1f overshoot=%.3f\n", name, response->reachMs, response->overshoot);

        EXPECT_NEAR(response->reachMs, 1000.0f * target / acceleration, 20.0f);
        EXPECT_LT(response->overshoot, 0.035f * target);
        EXPECT_TRUE(mil.Disable());
    }
}

namespace
{
    constexpr float detentTorqueCommand{ 0.12f };

    mil::Scenario BenchWithCoggingAndDryFriction()
    {
        mil::Scenario scenario;
        scenario.Plant("bench").Recorded(recordRateHz, 3000);
        sil::DescribeBenchFrictionAndCogging(scenario.plant);
        return scenario;
    }

    float TravelUnderTorque(mil::ModelInTheLoop& mil)
    {
        EXPECT_TRUE(mil.Enable());
        EXPECT_TRUE(mil.SetTorque(detentTorqueCommand));
        EXPECT_TRUE(mil.CaptureAfterSetpoint(2000.0f));

        const auto& samples = mil.Trace().Samples();
        const auto setpointTick = mil.SetpointTick();
        float first{ 0.0f };
        bool started{ false };
        float last{ 0.0f };
        for (const auto& sample : samples)
        {
            if (!setpointTick || sample.tick < *setpointTick)
                continue;
            if (!started)
            {
                first = sample.thetaMech;
                started = true;
            }
            last = sample.thetaMech;
        }

        EXPECT_TRUE(mil.Disable());
        return std::abs(last - first);
    }
}

TEST(TestDriveLimits, with_the_stored_cogging_map_a_small_torque_turns_the_rotor_through_the_detents)
{
    auto scenario = BenchWithCoggingAndDryFriction();
    scenario.BootsIn(state_machine::ControlMode::speed);
    mil::ModelInTheLoop mil{ scenario };
    ASSERT_TRUE(mil.Calibrate());
    ASSERT_GT(mil::Identified(mil.CalibrationRecord(), mil::Quantity::cogging), 0.0f);
    ASSERT_TRUE(mil.Select(state_machine::ControlMode::torque));
    ASSERT_TRUE(mil.Align());

    const auto travel = TravelUnderTorque(mil);
    std::fprintf(stderr, "[METRIC] mil limits torque_cogging compensated travel_rad=%.3f\n", travel);

    EXPECT_GT(travel, 2.0f * std::numbers::pi_v<float>);
}

TEST(TestDriveLimits, without_a_cogging_map_the_same_torque_stays_in_a_detent)
{
    auto scenario = BenchWithCoggingAndDryFriction();
    scenario.Calibrated().BootsIn(state_machine::ControlMode::torque);
    mil::ModelInTheLoop mil{ scenario };
    ASSERT_TRUE(mil.Align());

    const auto travel = TravelUnderTorque(mil);
    std::fprintf(stderr, "[METRIC] mil limits torque_cogging uncompensated travel_rad=%.3f\n", travel);

    EXPECT_LT(travel, 2.0f * std::numbers::pi_v<float> / 24.0f);
}

namespace
{
    constexpr float stoppedSpeed{ 2.0f };

    mil::Scenario QuickStopScenario(state_machine::ControlMode mode, float deceleration)
    {
        auto scenario = mil::Scenario{}.Calibrated().BootsIn(mode);
        scenario.nvm.config.limits.quickStopDeceleration = deceleration;
        scenario.nvm.config.limits.overspeedLimit = 105.0f;
        return scenario;
    }

    float PlantSpeed(mil::ModelInTheLoop& mil)
    {
        return mil.Drive().Model().Mechanics().Speed().Value();
    }

    float MillisecondsSince(mil::ModelInTheLoop& mil, uint32_t tick)
    {
        return static_cast<float>(mil.Drive().Tick() - tick) / static_cast<float>(mil.TicksFromMilliseconds(1.0f));
    }

    struct QuickStopResponse
    {
        float belowStoppedMs;
        float readyMs;
        bool stoppingWhileBraking;
    };

    std::optional<QuickStopResponse> QuickStop(mil::ModelInTheLoop& mil)
    {
        const auto start = mil.Drive().Tick();
        if (mil.ControlMode().Drive().ActiveStateMachine().CmdQuickStop() != state_machine::CommandResult::ok)
            return std::nullopt;

        const bool stopping = std::holds_alternative<state_machine::Stopping>(mil.ControlMode().Drive().ActiveStateMachine().CurrentState());
        if (!mil.RunUntil([&mil]()
                {
                    return std::abs(PlantSpeed(mil)) < stoppedSpeed || mil.IsReady();
                },
                std::chrono::seconds(2)))
            return std::nullopt;

        const auto belowStopped = MillisecondsSince(mil, start);
        if (!mil.RunUntil([&mil]()
                {
                    return mil.IsReady();
                },
                std::chrono::seconds(2)))
            return std::nullopt;

        return QuickStopResponse{ belowStopped, MillisecondsSince(mil, start), stopping };
    }
}

TEST(TestDriveLimits, a_quick_stop_at_500_rad_per_s2_brings_a_speed_mode_rotor_from_100_rad_per_s_below_2_rad_per_s_in_196_ms)
{
    mil::ModelInTheLoop mil{ QuickStopScenario(state_machine::ControlMode::speed, 500.0f) };
    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.HoldSpeed(100.0f, std::chrono::milliseconds(1000)));

    const auto response = QuickStop(mil);
    ASSERT_TRUE(response.has_value());
    std::fprintf(stderr, "[METRIC] mil limits quick_stop_speed below=%.1f ready=%.1f\n", response->belowStoppedMs, response->readyMs);

    EXPECT_TRUE(response->stoppingWhileBraking);
    EXPECT_NEAR(response->belowStoppedMs, 196.0f, 19.6f);
    EXPECT_LE(response->readyMs, response->belowStoppedMs + 5.0f);
    EXPECT_FALSE(mil.HasFault());
}

TEST(TestDriveLimits, a_quick_stop_that_would_take_longer_than_a_second_cuts_the_power_stage_after_one)
{
    mil::ModelInTheLoop mil{ QuickStopScenario(state_machine::ControlMode::speed, 50.0f) };
    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.HoldSpeed(100.0f, std::chrono::milliseconds(1000)));

    const auto start = mil.Drive().Tick();
    ASSERT_EQ(state_machine::CommandResult::ok, mil.ControlMode().Drive().ActiveStateMachine().CmdQuickStop());
    ASSERT_TRUE(mil.RunUntil([&mil]()
        {
            return mil.IsReady();
        },
        std::chrono::seconds(2)));
    const auto readyMs = MillisecondsSince(mil, start);
    std::fprintf(stderr, "[METRIC] mil limits quick_stop_timeout ready=%.1f speed=%.2f\n", readyMs, PlantSpeed(mil));

    EXPECT_NEAR(readyMs, 1000.0f, 5.0f);
    EXPECT_GT(PlantSpeed(mil), stoppedSpeed);
}

TEST(TestDriveLimits, a_quick_stop_brakes_a_torque_mode_rotor_against_its_speed)
{
    mil::ModelInTheLoop mil{ QuickStopScenario(state_machine::ControlMode::torque, 500.0f) };
    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.SetTorque(1.0f));
    mil.Run(std::chrono::milliseconds(500));
    const auto before = PlantSpeed(mil);

    const auto response = QuickStop(mil);
    ASSERT_TRUE(response.has_value());
    std::fprintf(stderr, "[METRIC] mil limits quick_stop_torque from=%.1f below=%.1f ready=%.1f\n", before, response->belowStoppedMs, response->readyMs);

    EXPECT_GT(before, 90.0f);
    EXPECT_TRUE(response->stoppingWhileBraking);
    EXPECT_LE(response->belowStoppedMs, (before - stoppedSpeed) / 500.0f * 1000.0f * 1.1f);
    EXPECT_LE(response->readyMs, response->belowStoppedMs + 11.0f);
    EXPECT_FALSE(mil.HasFault());
}
