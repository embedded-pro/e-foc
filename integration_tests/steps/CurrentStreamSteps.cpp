#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

using namespace integration;

namespace
{
    constexpr auto kStreamCollectionTimeout = std::chrono::seconds{ 60 };

    hal::Can::Id StreamId(uint16_t nodeId)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::telemetry, can::focMotorCategoryId, can::focTelemetryCurrentsId, nodeId));
    }

    StreamedCurrent Decode(const hal::Can::Message& frame)
    {
        services::CanPayloadReader reader{ frame };
        StreamedCurrent sample;
        sample.sequence = reader.ReadUInt8();
        sample.idAmpere = reader.ReadFixed16(can::focStreamCurrentScale);
        sample.iqAmpere = reader.ReadFixed16(can::focStreamCurrentScale);
        sample.positionRadian = reader.ReadFixed16(can::focStreamPositionScale);
        return sample;
    }

    uint32_t SequenceGaps(const std::vector<StreamedCurrent>& samples)
    {
        uint32_t gaps{ 0 };
        for (std::size_t i = 1; i < samples.size(); ++i)
            if (static_cast<uint8_t>(samples[i].sequence - samples[i - 1].sequence) != 1)
                ++gaps;
        return gaps;
    }

    float Mean(const std::vector<StreamedCurrent>& samples, float StreamedCurrent::*field)
    {
        float sum{ 0.0f };
        for (const auto& sample : samples)
            sum += sample.*field;
        return samples.empty() ? 0.0f : sum / static_cast<float>(samples.size());
    }
}

WHEN(R"(the telemetry rate is set to {int} Hz)", (int rateHz))
{
    auto& fixture = context.Get<Fixture>();
    services::CanPayloadWriter payload;
    payload.WriteUInt32(static_cast<uint32_t>(rateHz));
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focConfigureTelemetryRateId, payload.Message()))
        << "Telemetry rate " << rateHz << " Hz was refused";
}

WHEN(R"({int} streamed current samples are collected)", (int count))
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    const auto frames = fixture.CollectCapturedCanFrames(StreamId(fixture.ServerNodeId()), fixture.MarkCapture(), static_cast<std::size_t>(count), kStreamCollectionTimeout);
    setup.streamedCurrents.clear();
    for (const auto& frame : frames)
    {
        ASSERT_EQ(frame.size(), 7u) << "A TelemetryCurrents frame is seven bytes";
        setup.streamedCurrents.push_back(Decode(frame));
    }

    ASSERT_GE(setup.streamedCurrents.size(), static_cast<std::size_t>(count)) << "The stream stopped before " << count << " samples arrived";
}

THEN(R"(the streamed sequence shall have at most {int} gaps)", (int limit))
{
    const auto& samples = context.Get<ScenarioSetup>().streamedCurrents;
    const auto gaps = SequenceGaps(samples);
    std::printf("[METRIC] current stream samples=%zu gaps=%u\n", samples.size(), gaps);
    EXPECT_LE(gaps, static_cast<uint32_t>(limit));
}

THEN(R"(the mean streamed {word} current shall be within {float} A of {float} A)", (std::string axis, float tolerance, float expected))
{
    const auto& samples = context.Get<ScenarioSetup>().streamedCurrents;
    ASSERT_TRUE(axis == "d-axis" || axis == "q-axis") << "Unknown current axis: " << axis;
    const auto mean = Mean(samples, axis == "d-axis" ? &StreamedCurrent::idAmpere : &StreamedCurrent::iqAmpere);
    std::printf("[METRIC] current stream mean %s current=%.4f A expected=%.4f A\n", axis.c_str(), mean, expected);
    EXPECT_NEAR(mean, expected, tolerance);
}

THEN(R"(every streamed position shall lie within half a turn of the alignment zero)")
{
    for (const auto& sample : context.Get<ScenarioSetup>().streamedCurrents)
        EXPECT_LE(std::abs(sample.positionRadian), std::numbers::pi_v<float> + 1e-4f);
}

THEN(R"(no streamed current sample shall arrive within {int} ms)", (int milliseconds))
{
    auto& fixture = context.Get<Fixture>();
    const auto frames = fixture.CollectCapturedCanFrames(StreamId(fixture.ServerNodeId()), fixture.MarkCapture(), 1, std::chrono::milliseconds{ milliseconds });
    EXPECT_TRUE(frames.empty()) << frames.size() << " streamed samples arrived although the stream is stopped";
}
