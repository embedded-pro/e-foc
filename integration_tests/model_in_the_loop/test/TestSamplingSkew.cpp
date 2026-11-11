#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <vector>

namespace
{
    constexpr uint32_t plantSampleRateHz{ 5000 };
    constexpr uint32_t plantMaxSamples{ 20000 };
    constexpr float overspeedLimit{ 600.0f };
    constexpr uint32_t spinUpMs{ 1500 };
    constexpr uint32_t windowMs{ 200 };

    struct Means
    {
        float id{ 0.0f };
        float iq{ 0.0f };
        float speed{ 0.0f };
    };

    struct Error
    {
        float quadrature;
        float direct;
        float speed;
    };

    mil::Scenario TeknicThroughTheBenchSenseChain(bool corrected)
    {
        auto scenario = mil::Scenario{}.Plant("teknic");
        sil::DescribeBenchCurrentSense(scenario.plant);
        if (!corrected)
            scenario.WithoutSamplingSkewCorrection();
        scenario.Recorded(plantSampleRateHz, plantMaxSamples).Calibrated().BootsIn(state_machine::ControlMode::torque);
        scenario.nvm.config.limits.overspeedLimit = overspeedLimit;
        return scenario;
    }

    Means StreamMeans(mil::ModelInTheLoop& mil, uint32_t milliseconds)
    {
        Means sums;
        uint32_t count{ 0 };
        for (uint32_t i = 0; i != milliseconds; ++i)
        {
            mil.Run(std::chrono::milliseconds{ 1 });
            while (const auto sample = mil.CurrentStream().Take())
            {
                sums.id += sample->meanId.Value();
                sums.iq += sample->meanIq.Value();
                ++count;
            }
        }

        return count == 0 ? Means{} : Means{ sums.id / static_cast<float>(count), sums.iq / static_cast<float>(count) };
    }

    Means PlantMeans(mil::ModelInTheLoop& mil, uint32_t fromTick, uint32_t toTick)
    {
        Means sums;
        uint32_t count{ 0 };
        for (const auto& sample : mil.Trace().Samples())
            if (sample.tick >= fromTick && sample.tick < toTick)
            {
                sums.id += sample.id;
                sums.iq += sample.iq;
                sums.speed += sample.omegaMech;
                ++count;
            }

        return count == 0 ? Means{} : Means{ sums.id / static_cast<float>(count), sums.iq / static_cast<float>(count), sums.speed / static_cast<float>(count) };
    }

    Error MeasuredAgainstTrue(bool corrected)
    {
        mil::ModelInTheLoop mil{ TeknicThroughTheBenchSenseChain(corrected) };
        EXPECT_TRUE(mil.Align());
        EXPECT_TRUE(mil.Enable());
        EXPECT_TRUE(mil.SetTorque(1.0f));
        mil.Run(std::chrono::milliseconds{ spinUpMs });

        mil.CurrentStream().Start(mil.TicksFromMilliseconds(1.0f));
        (void)StreamMeans(mil, 5);
        const auto fromTick = mil.Drive().Tick();
        const auto measured = StreamMeans(mil, windowMs);
        const auto plant = PlantMeans(mil, fromTick, mil.Drive().Tick());

        EXPECT_TRUE(mil.IsRunning());
        return { measured.iq - plant.iq, measured.id - plant.id, plant.speed };
    }

    void Report(const char* label, const Error& error)
    {
        std::fprintf(stderr, "[METRIC] mil sampling_skew %s speed=%.1f iq_error_A=%.4f id_error_A=%.4f\n", label, error.speed, error.quadrature, error.direct);
    }
}

TEST(TestSamplingSkew, uncorrected_the_low_inductance_motor_at_speed_reads_its_q_axis_current_low_by_the_back_emf_slope)
{
    const auto error = MeasuredAgainstTrue(false);
    Report("uncorrected", error);

    EXPECT_GT(error.speed, 0.8f * overspeedLimit);
    EXPECT_LT(error.quadrature, -0.15f);
}

TEST(TestSamplingSkew, corrected_the_low_inductance_motor_at_speed_reads_the_current_it_carries)
{
    const auto error = MeasuredAgainstTrue(true);
    Report("corrected", error);

    EXPECT_GT(error.speed, 0.8f * overspeedLimit);
    EXPECT_LT(std::abs(error.quadrature), 0.01f);
    EXPECT_LT(std::abs(error.direct), 0.01f);
}
