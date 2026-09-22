#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/response/ResponseWindow.hpp"
#include "integration_tests/support/response/StepMetrics.hpp"
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>

using namespace integration;
using namespace integration::response;

namespace
{
    constexpr auto kCaptureWallClockTimeout = std::chrono::seconds{ 120 };
    constexpr float kMilliPerSecond = 1000.0f;

    uint32_t CommandRawId(uint8_t messageType)
    {
        return services::MakeCanId(services::CanPriority::command, can::focMotorCategoryId, messageType, Fixture::kServerNodeId);
    }

    uint32_t TicksFromMilliseconds(const ScenarioSetup& setup, float milliseconds)
    {
        return static_cast<uint32_t>(milliseconds * static_cast<float>(setup.plant.baseFrequencyHz) / kMilliPerSecond);
    }

    float SecondsPerSample(const ScenarioSetup& setup)
    {
        const auto spacing = setup.trace.TickSpacing();
        return static_cast<float>(spacing.value_or(1)) / static_cast<float>(setup.plant.baseFrequencyHz);
    }

    void FeedNewLines(ScenarioSetup& setup)
    {
        const auto& lines = TargetInteractor::Instance().SerialLines();
        for (; setup.consumedLines < lines.size(); ++setup.consumedLines)
            setup.trace.Consume(lines[setup.consumedLines]);
    }

    template<typename Predicate>
    bool CaptureUntil(ScenarioSetup& setup, Predicate done)
    {
        auto& interactor = TargetInteractor::Instance();
        const auto deadline = std::chrono::steady_clock::now() + kCaptureWallClockTimeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            FeedNewLines(setup);
            if (done())
                return true;
            interactor.DrainSerial(std::chrono::milliseconds{ 50 });
        }

        FeedNewLines(setup);
        return done();
    }

    std::optional<uint32_t> SetpointStampAfterEnable(const ScenarioSetup& setup)
    {
        if (!setup.lastSetpoint || !setup.trace.StartTick())
            return std::nullopt;

        const auto stamp = setup.trace.CommandTick(CommandRawId(setup.lastSetpoint->messageType));
        if (stamp && *stamp >= *setup.trace.StartTick())
            return stamp;
        return std::nullopt;
    }

    Signal RequireSignal(const std::string& word)
    {
        const auto signal = ParseSignal(word);
        EXPECT_TRUE(signal.has_value()) << "Unknown response signal: " << word;
        return signal.value_or(Signal::speed);
    }

    void RequireCapturedTrace(const ScenarioSetup& setup)
    {
        ASSERT_TRUE(setup.trace.StartTick().has_value()) << "No PLANT_START marker: was the plant response recorded and the motor enabled?";
        ASSERT_TRUE(setup.lastSetpoint.has_value()) << "No setpoint was applied";
        ASSERT_TRUE(setup.trace.TickSpacing().has_value()) << "Fewer than two plant samples captured";
    }

    struct StepWindow
    {
        uint32_t onsetTick;
        float initial;
        float reference;
        std::vector<float> normalised;
    };

    std::optional<StepWindow> ExtractStepWindow(const ScenarioSetup& setup, Signal signal)
    {
        const auto stamp = SetpointStampAfterEnable(setup);
        const uint32_t onset = stamp.value_or(*setup.trace.StartTick());
        const float initial = stamp ? ValueAt(setup.trace, signal, onset).value_or(0.0f) : 0.0f;
        const float reference = setup.lastSetpoint->value;
        const auto raw = Extract(setup.trace, signal, onset, StepWindowSamples(signal));

        if (raw.size() < StepWindowSamples(signal))
            return std::nullopt;

        return StepWindow{ onset, initial, reference, NormaliseStep(raw, initial, reference) };
    }

    std::optional<StepResponseMetrics> StepMetricsFor(const ScenarioSetup& setup, Signal signal, float bandPercent, const char* algorithmLabel)
    {
        const auto window = ExtractStepWindow(setup, signal);
        if (!window)
        {
            ADD_FAILURE() << "Captured fewer than " << StepWindowSamples(signal) << " " << SignalName(signal) << " samples after the step";
            return std::nullopt;
        }

        const float dt = SecondsPerSample(setup);
        const auto metrics = ComputeStepMetricsFor(signal, window->normalised, dt, bandPercent / 100.0f, window->reference - window->initial);
        if (!metrics)
            return std::nullopt;

        std::fprintf(stderr, "[METRIC] %s step %s onset_tick=%lu from=%.4g to=%.4g band_pct=%.1f rise_ms=%.3f settle_ms=%.3f overshoot_pct=%.2f peak_ms=%.3f tail_band_pct=%.2f ss_err=%.4g %s\n",
            SignalName(signal), algorithmLabel,
            static_cast<unsigned long>(window->onsetTick), window->initial, window->reference, bandPercent,
            metrics->riseTimeS * kMilliPerSecond, metrics->settlingTimeS * kMilliPerSecond,
            metrics->percentOvershoot, metrics->peakTimeS * kMilliPerSecond, metrics->tailBandPercent,
            metrics->steadyStateError, SignalUnit(signal));
        return metrics;
    }

    std::optional<DisturbanceResponseMetrics> DisturbanceMetricsFor(const ScenarioSetup& setup, Signal signal, float absoluteBand)
    {
        const auto onset = setup.trace.EventTick("torque");
        if (!onset)
        {
            ADD_FAILURE() << "No torque step event in the captured response";
            return std::nullopt;
        }

        const auto raw = Extract(setup.trace, signal, *onset, disturbanceWindowSamples);
        if (raw.size() < disturbanceWindowSamples)
        {
            ADD_FAILURE() << "Captured fewer than " << disturbanceWindowSamples << " " << SignalName(signal) << " samples after the torque step";
            return std::nullopt;
        }

        const float reference = setup.lastSetpoint->value;
        const auto metrics = ComputeDisturbanceMetrics<disturbanceWindowSamples>(raw, reference, SecondsPerSample(setup), absoluteBand);
        if (!metrics)
            return std::nullopt;

        std::fprintf(stderr, "[METRIC] %s disturbance onset_tick=%lu reference=%.4g max_deviation=%.4g recovery_ms=%.3f ss_err=%.4g %s\n",
            SignalName(signal), static_cast<unsigned long>(*onset), reference,
            metrics->maxDeviation, metrics->recoveryTimeS * kMilliPerSecond, metrics->steadyStateError, SignalUnit(signal));
        return metrics;
    }

    const char* AlgorithmLabel(const ScenarioSetup& setup, Signal signal)
    {
        static const char* const current[] = { "pid", "decoupled", "deadbeat", "sliding" };
        static const char* const speed[] = { "pid", "lqi", "adrc", "twodof" };
        static const char* const position[] = { "pid", "cascadep", "lqr", "lqi", "twodof" };

        switch (signal)
        {
            case Signal::currentQ:
                return setup.nvm.config.currentAlgorithm < 4 ? current[setup.nvm.config.currentAlgorithm] : "?";
            case Signal::speed:
                return setup.nvm.config.speedAlgorithm < 4 ? speed[setup.nvm.config.speedAlgorithm] : "?";
            case Signal::position:
                return setup.nvm.config.positionAlgorithm < 5 ? position[setup.nvm.config.positionAlgorithm] : "?";
        }
        return "?";
    }
}

GIVEN(R"(the plant response is recorded at {int} Hz for up to {int} samples)", (int sampleRateHz, int maxSamples))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Plant response recording needs a simulated target";
    ASSERT_FALSE(setup.booted) << "Recording must be configured before the target boots";
    ASSERT_GT(sampleRateHz, 0);
    ASSERT_EQ(setup.plant.baseFrequencyHz % static_cast<uint32_t>(sampleRateHz), 0u) << "Sample rate must divide the control frequency";

    setup.plant.responseSampleRateHz = static_cast<uint32_t>(sampleRateHz);
    setup.plant.responseMaxSamples = static_cast<uint32_t>(maxSamples);
}

GIVEN(R"(a torque step of {float} Nm applied {int} ms after enable)", (float torqueNm, int delayMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Torque steps need a simulated target";
    ASSERT_FALSE(setup.booted) << "The torque step must be scheduled before the target boots";

    setup.plant.torqueStepNm = torqueNm;
    setup.plant.torqueStepDelayMs = static_cast<uint32_t>(delayMs);
}

GIVEN(R"(the encoder freezes {int} ms after enable)", (int delayMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Freezing the encoder needs a simulated target";
    ASSERT_FALSE(setup.booted) << "The encoder freeze must be scheduled before the target boots";
    ASSERT_GT(delayMs, 0) << "A zero delay is how a scenario says the encoder never freezes while running";
    ASSERT_EQ(delayMs % 10, 0) << "The freeze delay is carried in centiseconds and must be a whole number of them";
    ASSERT_LE(delayMs, 2550) << "The freeze delay must fit in the byte the plant record carries it in";

    setup.plant.encoderFreezeDelayCentiseconds = static_cast<uint8_t>(delayMs / 10);
}

WHEN(R"(the response is captured for {int} ms after enable)", (int milliseconds))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto window = TicksFromMilliseconds(setup, static_cast<float>(milliseconds));

    ASSERT_TRUE(CaptureUntil(setup, [&]
        {
            const auto start = setup.trace.StartTick();
            const auto last = setup.trace.LastSampleTick();
            return start && ((last && *last >= *start + window) || setup.trace.StopTick());
        }))
        << "Plant response did not reach " << milliseconds << " ms after enable";
}

WHEN(R"(the response is captured for {int} ms after the last setpoint)", (int milliseconds))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(setup.lastSetpoint.has_value()) << "No setpoint was applied";
    const auto window = TicksFromMilliseconds(setup, static_cast<float>(milliseconds));

    ASSERT_TRUE(CaptureUntil(setup, [&]
        {
            const auto stamp = SetpointStampAfterEnable(setup);
            const auto last = setup.trace.LastSampleTick();
            return stamp && ((last && *last >= *stamp + window) || setup.trace.StopTick());
        }))
        << "Plant response did not reach " << milliseconds << " ms after the last setpoint";
}

THEN(R"(the {word} step response shall settle into a {float} % band within {float} ms with overshoot below {float} %)", (std::string signalWord, float bandPercent, float settleMs, float overshootPercent))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);

    const auto metrics = StepMetricsFor(setup, signal, bandPercent, AlgorithmLabel(setup, signal));
    ASSERT_TRUE(metrics.has_value());

    EXPECT_LE(metrics->settlingTimeS * kMilliPerSecond, settleMs) << SignalName(signal) << " settled too slowly";
    EXPECT_LT(metrics->percentOvershoot, overshootPercent) << SignalName(signal) << " overshoot too large";
}

THEN(R"(the {word} step response shall rise within {float} ms)", (std::string signalWord, float riseMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);

    const auto metrics = StepMetricsFor(setup, signal, 2.0f, AlgorithmLabel(setup, signal));
    ASSERT_TRUE(metrics.has_value());

    EXPECT_LE(metrics->riseTimeS * kMilliPerSecond, riseMs) << SignalName(signal) << " rose too slowly";
}

THEN(R"(the {word} response tail shall stay within {float} % of the setpoint)", (std::string signalWord, float tailPercent))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);

    const auto metrics = StepMetricsFor(setup, signal, 2.0f, AlgorithmLabel(setup, signal));
    ASSERT_TRUE(metrics.has_value());

    EXPECT_LE(metrics->tailBandPercent, tailPercent) << SignalName(signal) << " keeps rippling outside its tail band";
}

THEN(R"(the steady-state {word} error shall be below {float} {word})", (std::string signalWord, float limit, std::string unit))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);
    ASSERT_EQ(unit, SignalUnit(signal)) << "Unit does not match the signal";

    const auto metrics = StepMetricsFor(setup, signal, 2.0f, AlgorithmLabel(setup, signal));
    ASSERT_TRUE(metrics.has_value());

    EXPECT_LT(std::abs(metrics->steadyStateError), limit) << SignalName(signal) << " steady-state error too large";
}

THEN(R"(the {word} deviation after the torque step shall stay below {float} {word})", (std::string signalWord, float limit, std::string unit))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);
    ASSERT_EQ(unit, SignalUnit(signal)) << "Unit does not match the signal";

    const auto metrics = DisturbanceMetricsFor(setup, signal, limit);
    ASSERT_TRUE(metrics.has_value());

    EXPECT_LT(metrics->maxDeviation, limit) << SignalName(signal) << " deviated too far after the torque step";
}

THEN(R"(the {word} shall recover to within {float} {word} of the setpoint within {float} ms of the torque step)", (std::string signalWord, float band, std::string unit, float recoveryMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);
    ASSERT_EQ(unit, SignalUnit(signal)) << "Unit does not match the signal";

    const auto metrics = DisturbanceMetricsFor(setup, signal, band);
    ASSERT_TRUE(metrics.has_value());

    EXPECT_LE(metrics->recoveryTimeS * kMilliPerSecond, recoveryMs) << SignalName(signal) << " did not recover in time";
    EXPECT_LT(std::abs(metrics->steadyStateError), band) << SignalName(signal) << " did not return to the setpoint";
}

THEN(R"(the response shall have no dropped samples)")
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedNewLines(setup);

    EXPECT_EQ(setup.trace.Dropped(), 0u) << "The recorder dropped samples";
    EXPECT_EQ(setup.trace.Gaps(), 0u) << "The captured samples are not uniformly spaced";
}
