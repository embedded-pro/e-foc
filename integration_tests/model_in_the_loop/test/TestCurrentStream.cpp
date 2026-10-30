#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

namespace
{
    constexpr uint32_t plantSampleRateHz{ 5000 };
    constexpr uint32_t plantMaxSamples{ 10000 };

    std::vector<foc::CurrentStreamSample> StreamFor(mil::ModelInTheLoop& mil, uint32_t milliseconds)
    {
        std::vector<foc::CurrentStreamSample> samples;
        for (uint32_t i = 0; i != milliseconds; ++i)
        {
            mil.Run(std::chrono::milliseconds{ 1 });
            while (const auto sample = mil.CurrentStream().Take())
                samples.push_back(*sample);
        }
        return samples;
    }

    uint32_t SequenceGaps(const std::vector<foc::CurrentStreamSample>& samples)
    {
        uint32_t gaps{ 0 };
        for (std::size_t i = 1; i < samples.size(); ++i)
            if (static_cast<uint8_t>(samples[i].sequence - samples[i - 1].sequence) != 1)
                ++gaps;
        return gaps;
    }

    struct Means
    {
        float id{ 0.0f };
        float iq{ 0.0f };
    };

    Means StreamMeans(const std::vector<foc::CurrentStreamSample>& samples)
    {
        Means means;
        for (const auto& sample : samples)
        {
            means.id += sample.meanId.Value() / static_cast<float>(samples.size());
            means.iq += sample.meanIq.Value() / static_cast<float>(samples.size());
        }
        return means;
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
                ++count;
            }

        if (count == 0)
            return {};
        return { sums.id / static_cast<float>(count), sums.iq / static_cast<float>(count) };
    }

    void DrainStartUp(mil::ModelInTheLoop& mil)
    {
        (void)StreamFor(mil, 5);
    }
}

TEST(TestCurrentStream, a_torque_run_streams_one_contiguous_window_mean_of_the_plant_currents_per_millisecond)
{
    mil::ModelInTheLoop mil{ mil::Scenario{}.Recorded(plantSampleRateHz, plantMaxSamples).Calibrated().BootsIn(state_machine::ControlMode::torque) };
    ASSERT_TRUE(mil.Align());
    mil.CurrentStream().Start(mil.TicksFromMilliseconds(1.0f));
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.SetTorque(0.5f));
    DrainStartUp(mil);

    const auto fromTick = mil.Drive().Tick();
    const auto samples = StreamFor(mil, 100);
    const auto stream = StreamMeans(samples);
    const auto plant = PlantMeans(mil, fromTick, mil.Drive().Tick());

    std::printf("[METRIC] mil current stream torque: samples=%zu gaps=%u stream_id=%.4f plant_id=%.4f stream_iq=%.4f plant_iq=%.4f\n",
        samples.size(), SequenceGaps(samples), stream.id, plant.id, stream.iq, plant.iq);
    EXPECT_EQ(samples.size(), 100u);
    EXPECT_EQ(SequenceGaps(samples), 0u);
    EXPECT_NEAR(stream.iq, plant.iq, 0.01f);
    EXPECT_NEAR(stream.id, plant.id, 0.01f);
    for (const auto& sample : samples)
        EXPECT_LE(std::abs(sample.position.Value()), std::numbers::pi_v<float> + 1e-4f);
}

TEST(TestCurrentStream, a_stopped_drive_streams_no_windows)
{
    mil::ModelInTheLoop mil{ mil::Scenario{}.Calibrated().BootsIn(state_machine::ControlMode::torque) };
    ASSERT_TRUE(mil.Align());
    mil.CurrentStream().Start(mil.TicksFromMilliseconds(1.0f));

    EXPECT_TRUE(StreamFor(mil, 20).empty());
}

TEST(TestCurrentStream, a_speed_run_streams_the_currents_of_the_inner_loop)
{
    mil::ModelInTheLoop mil{ mil::Scenario{}.Recorded(plantSampleRateHz, plantMaxSamples).Calibrated().BootsIn(state_machine::ControlMode::speed) };
    ASSERT_TRUE(mil.Align());
    mil.CurrentStream().Start(mil.TicksFromMilliseconds(1.0f));
    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.SetSpeed(20.0f));
    DrainStartUp(mil);

    const auto fromTick = mil.Drive().Tick();
    const auto samples = StreamFor(mil, 50);
    const auto stream = StreamMeans(samples);
    const auto plant = PlantMeans(mil, fromTick, mil.Drive().Tick());

    std::printf("[METRIC] mil current stream speed: samples=%zu gaps=%u stream_iq=%.4f plant_iq=%.4f\n", samples.size(), SequenceGaps(samples), stream.iq, plant.iq);
    EXPECT_EQ(samples.size(), 50u);
    EXPECT_EQ(SequenceGaps(samples), 0u);
    EXPECT_NEAR(stream.iq, plant.iq, 0.01f);
}
