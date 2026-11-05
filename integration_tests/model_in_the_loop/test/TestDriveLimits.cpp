#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>

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
