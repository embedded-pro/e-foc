#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>

namespace
{
    using namespace std::chrono_literals;

    constexpr uint32_t recordRateHz{ 1000 };
    constexpr float startWindowMs{ 300.0f };

    mil::Scenario BenchWithDryFriction(float storedFrictionFactor)
    {
        mil::Scenario scenario;
        scenario.Plant("bench").Recorded(recordRateHz, 3000);
        scenario.plant.coulombFrictionNm = 0.0037f;
        scenario.Calibrated().StoredCalibrationOffBy("dryFriction", storedFrictionFactor).BootsIn(state_machine::ControlMode::speed);
        return scenario;
    }

    float IntegratedSpeedError(mil::ModelInTheLoop& mil, float reference)
    {
        const auto setpointTick = mil.SetpointTick();
        if (!setpointTick)
            return -1.0f;

        const auto windowEnd = *setpointTick + mil.TicksFromMilliseconds(startWindowMs);
        float integral{ 0.0f };
        for (const auto& sample : mil.Trace().Samples())
            if (sample.tick >= *setpointTick && sample.tick < windowEnd)
                integral += std::abs(sample.omegaMech - reference) / static_cast<float>(recordRateHz);

        return integral;
    }

    float StartError(float storedFrictionFactor, float speed)
    {
        mil::ModelInTheLoop mil{ BenchWithDryFriction(storedFrictionFactor) };
        EXPECT_TRUE(mil.Align());
        EXPECT_TRUE(mil.Enable());
        mil.Run(100ms);
        EXPECT_TRUE(mil.SetSpeed(speed));
        EXPECT_TRUE(mil.CaptureAfterSetpoint(startWindowMs));
        mil::ExpectNoDroppedSamples(mil);

        return IntegratedSpeedError(mil, speed);
    }

    void ExpectFeedForwardLowersTheStartError(float speed)
    {
        SCOPED_TRACE(speed);
        const auto with = StartError(1.0f, speed);
        const auto without = StartError(0.0f, speed);
        std::fprintf(stderr, "[METRIC] mil friction start speed_rad_s=%.1f with_rad=%.4f without_rad=%.4f\n", speed, with, without);

        EXPECT_GT(with, 0.0f);
        EXPECT_LT(with, 0.75f * without);
    }
}

TEST(TestFrictionFeedForward, a_slow_forward_start_on_the_bench_misses_less_speed_with_the_stored_dry_friction)
{
    ExpectFeedForwardLowersTheStartError(3.0f);
}

TEST(TestFrictionFeedForward, a_slow_reverse_start_on_the_bench_misses_less_speed_with_the_stored_dry_friction)
{
    ExpectFeedForwardLowersTheStartError(-3.0f);
}
