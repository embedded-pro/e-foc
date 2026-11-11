#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/response/ResponseWindow.hpp"
#include "integration_tests/support/response/StepMetrics.hpp"
#include "numerical/motion_profiles/TrapezoidalProfileFromState.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <string>

using namespace integration;
using namespace integration::response;

namespace
{
    constexpr auto kCaptureWallClockTimeout = std::chrono::seconds{ 120 };
    constexpr float kMilliPerSecond = 1000.0f;
    constexpr float kMilliPerAmpere = 1000.0f;

    uint32_t CommandRawId(const Setpoint& setpoint)
    {
        return services::MakeCanId(services::CanPriority::command, can::focMotorCategoryId, setpoint.messageType, setpoint.nodeId);
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

    void FeedRecord(ScenarioSetup& setup, std::chrono::milliseconds timeout)
    {
        for (const auto& line : TargetInteractor::Instance().TakePlantRecord(timeout))
            setup.trace.Consume(line);
    }

    bool RecorderFull(const ScenarioSetup& setup)
    {
        return setup.plant.responseMaxSamples != 0 && setup.trace.Samples().size() >= setup.plant.responseMaxSamples;
    }

    template<typename Predicate>
    bool CaptureUntil(Fixture& fixture, ScenarioSetup& setup, Predicate done)
    {
        const auto deadline = std::chrono::steady_clock::now() + kCaptureWallClockTimeout;
        auto nextHeartbeat = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() < deadline)
        {
            FeedRecord(setup, std::chrono::milliseconds{ 0 });
            if (done())
                return true;
            // A capture can outlast the client timeout; a silent client would have the drive stop itself.
            if (std::chrono::steady_clock::now() >= nextHeartbeat)
            {
                fixture.SendClientHeartbeat();
                nextHeartbeat = std::chrono::steady_clock::now() + Fixture::kClientHeartbeatInterval;
            }
            FeedRecord(setup, std::chrono::milliseconds{ 50 });
        }

        FeedRecord(setup, std::chrono::milliseconds{ 0 });
        return done();
    }

    std::optional<uint32_t> SetpointStampAfterEnable(const ScenarioSetup& setup)
    {
        if (!setup.lastSetpoint || !setup.trace.StartTick())
            return std::nullopt;

        const auto& commands = setup.trace.Commands();
        const auto rawId = CommandRawId(*setup.lastSetpoint);

        for (auto index = commands.size(); index > setup.lastSetpoint->commandsBefore; --index)
            if (commands[index - 1].rawId == rawId)
                return commands[index - 1].tick >= *setup.trace.StartTick() ? std::optional{ commands[index - 1].tick } : std::nullopt;

        return std::nullopt;
    }

    Signal RequireSignal(const std::string& word)
    {
        const auto signal = ParseSignal(word);
        EXPECT_TRUE(signal.has_value()) << "Unknown response signal: " << word;
        return signal.value_or(Signal::speed);
    }

    std::optional<float> DirectCurrentRmsMilliAmpere(const ScenarioSetup& setup, int fromMs, int toMs)
    {
        const auto start = setup.trace.StartTick();
        if (!start)
            return std::nullopt;

        const auto from = *start + TicksFromMilliseconds(setup, static_cast<float>(fromMs));
        const auto to = *start + TicksFromMilliseconds(setup, static_cast<float>(toMs));
        float sumOfSquares{ 0.0f };
        std::size_t count{ 0 };
        for (const auto& sample : setup.trace.Samples())
            if (sample.tick >= from && sample.tick < to)
            {
                sumOfSquares += sample.id * sample.id;
                ++count;
            }

        if (count == 0)
            return std::nullopt;
        return std::sqrt(sumOfSquares / static_cast<float>(count)) * kMilliPerAmpere;
    }

    std::optional<float> QuadratureCurrentMean(const ScenarioSetup& setup, int fromMs, int toMs)
    {
        const auto start = setup.trace.StartTick();
        if (!start)
            return std::nullopt;

        const auto from = *start + TicksFromMilliseconds(setup, static_cast<float>(fromMs));
        const auto to = *start + TicksFromMilliseconds(setup, static_cast<float>(toMs));
        float sum{ 0.0f };
        std::size_t count{ 0 };
        for (const auto& sample : setup.trace.Samples())
            if (sample.tick >= from && sample.tick < to)
            {
                sum += sample.iq;
                ++count;
            }

        if (count == 0)
            return std::nullopt;
        return sum / static_cast<float>(count);
    }

    std::optional<float> TravelAfterSetpoint(const ScenarioSetup& setup)
    {
        const auto stamp = SetpointStampAfterEnable(setup);
        if (!stamp)
            return std::nullopt;

        std::optional<float> first;
        float last{ 0.0f };
        for (const auto& sample : setup.trace.Samples())
            if (sample.tick >= *stamp)
            {
                if (!first)
                    first = sample.thetaMech;
                last = sample.thetaMech;
            }

        if (!first)
            return std::nullopt;
        return std::abs(last - *first);
    }

    struct ProfileLimits
    {
        float velocity;
        float acceleration;
    };

    float ProfileFollowingError(const ScenarioSetup& setup, uint32_t stamp, float start, float goal, const ProfileLimits& limits, float offsetMs, float fromMs, float toMs)
    {
        const motion_profiles::TrapezoidalProfileFromState<float> profile{ start, 0.0f, goal, { limits.velocity, limits.acceleration, limits.acceleration } };
        const float ticksPerSecond = static_cast<float>(setup.plant.baseFrequencyHz);
        float worst{ 0.0f };

        for (const auto& sample : setup.trace.Samples())
        {
            if (sample.tick < stamp)
                continue;

            const float elapsedMs = static_cast<float>(sample.tick - stamp) / ticksPerSecond * kMilliPerSecond - offsetMs;
            if (elapsedMs >= fromMs && elapsedMs <= toMs)
                worst = std::max(worst, std::abs(sample.thetaMech - profile.Sample(elapsedMs / kMilliPerSecond).position));
        }

        return worst;
    }

    std::optional<float> PositionAtTick(const ScenarioSetup& setup, uint32_t tick)
    {
        for (const auto& sample : setup.trace.Samples())
            if (sample.tick >= tick)
                return sample.thetaMech;

        return std::nullopt;
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

GIVEN(R"(the motor cable is pulled {int} ms after enable)", (int delayMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Pulling the cable needs a simulated target";
    ASSERT_FALSE(setup.booted) << "The cable pull must be scheduled before the target boots";
    ASSERT_GT(delayMs, 0) << "A zero delay is how a scenario says the cable is never pulled";

    setup.plant.cableDisconnectDelayMs = static_cast<uint32_t>(delayMs);
}

GIVEN(R"(the supply steps to {float} of its voltage {int} ms after enable)", (float scale, int delayMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Stepping the supply needs a simulated target";
    ASSERT_FALSE(setup.booted) << "The supply step must be scheduled before the target boots";
    ASSERT_GT(scale, 0.0f) << "A zero scale is how a scenario says the supply never steps";

    setup.plant.supplyStepScale = scale;
    setup.plant.supplyStepDelayMs = static_cast<uint32_t>(delayMs);
}

GIVEN(R"(a hand turns the shaft by {float} rad over {int} ms, {int} ms after enable, and holds it)", (float radians, int turnMs, int delayMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Gripping the shaft needs a simulated target";
    ASSERT_FALSE(setup.booted) << "The grip must be scheduled before the target boots";
    ASSERT_NE(radians, 0.0f) << "A zero angle is how a scenario says no hand grips the shaft";
    ASSERT_GT(turnMs, 0);
    ASSERT_GE(delayMs, 0);

    setup.plant.shaftTurnRadians = radians;
    setup.plant.shaftTurnMs = static_cast<uint32_t>(turnMs);
    setup.plant.shaftGripDelayMs = static_cast<uint32_t>(delayMs);
}

WHEN(R"(the response is captured for {int} ms after enable)", (int milliseconds))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto window = TicksFromMilliseconds(setup, static_cast<float>(milliseconds));

    ASSERT_TRUE(CaptureUntil(context.Get<Fixture>(), setup, [&]
        {
            const auto start = setup.trace.StartTick();
            const auto last = setup.trace.LastSampleTick();
            return start && ((last && *last >= *start + window) || setup.trace.StopTick() || RecorderFull(setup));
        }))
        << "Plant response did not reach " << milliseconds << " ms after enable";
}

WHEN(R"(the response is captured for {int} ms after the last setpoint)", (int milliseconds))
{
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(setup.lastSetpoint.has_value()) << "No setpoint was applied";
    const auto window = TicksFromMilliseconds(setup, static_cast<float>(milliseconds));

    ASSERT_TRUE(CaptureUntil(context.Get<Fixture>(), setup, [&]
        {
            const auto stamp = SetpointStampAfterEnable(setup);
            const auto last = setup.trace.LastSampleTick();
            return stamp && ((last && *last >= *stamp + window) || setup.trace.StopTick() || RecorderFull(setup));
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

THEN(R"(the {word} step response shall take at least {float} ms to rise)", (std::string signalWord, float riseMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    RequireCapturedTrace(setup);
    const auto signal = RequireSignal(signalWord);

    const auto metrics = StepMetricsFor(setup, signal, 2.0f, AlgorithmLabel(setup, signal));
    ASSERT_TRUE(metrics.has_value());

    EXPECT_GE(metrics->riseTimeS * kMilliPerSecond, riseMs) << SignalName(signal) << " rose faster than its ramp allows";
}

THEN(R"(the {word} response tail shall stay within {float} % of the step)", (std::string signalWord, float tailPercent))
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

THEN(R"(the d-axis current shall be above {int} mA RMS from {int} to {int} ms after enable)", (int limitMa, int fromMs, int toMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    const auto rms = DirectCurrentRmsMilliAmpere(setup, fromMs, toMs);
    ASSERT_TRUE(rms.has_value()) << "No plant samples between " << fromMs << " and " << toMs << " ms after enable";
    std::fprintf(stderr, "[METRIC] sil id_rms_ma from_ms=%d to_ms=%d value=%.2f limit=%d\n", fromMs, toMs, *rms, limitMa);
    EXPECT_GT(*rms, static_cast<float>(limitMa)) << "d-axis current " << *rms << " mA RMS between " << fromMs << " and " << toMs << " ms";
}

THEN(R"(the d-axis current shall be below {int} mA RMS from {int} to {int} ms after enable)", (int limitMa, int fromMs, int toMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    const auto rms = DirectCurrentRmsMilliAmpere(setup, fromMs, toMs);
    ASSERT_TRUE(rms.has_value()) << "No plant samples between " << fromMs << " and " << toMs << " ms after enable";
    std::fprintf(stderr, "[METRIC] sil id_rms_ma from_ms=%d to_ms=%d value=%.2f limit=%d\n", fromMs, toMs, *rms, limitMa);
    EXPECT_LT(*rms, static_cast<float>(limitMa)) << "d-axis current " << *rms << " mA RMS between " << fromMs << " and " << toMs << " ms";
}

THEN(R"(the peak speed shall stay below {float} rad\/s)", (float limit))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    ASSERT_FALSE(setup.trace.Samples().empty()) << "No plant samples captured";
    float peak{ 0.0f };
    for (const auto& sample : setup.trace.Samples())
        peak = std::max(peak, std::abs(sample.omegaMech));

    std::fprintf(stderr, "[METRIC] sil peak_speed value=%.3f limit=%.3f\n", peak, limit);
    EXPECT_LT(peak, limit) << "The rotor reached " << peak << " rad/s";
}

THEN(R"(the q-axis current shall average between {float} and {float} A from {int} to {int} ms after enable)", (float low, float high, int fromMs, int toMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    const auto mean = QuadratureCurrentMean(setup, fromMs, toMs);
    ASSERT_TRUE(mean.has_value()) << "No plant samples between " << fromMs << " and " << toMs << " ms after enable";
    std::fprintf(stderr, "[METRIC] sil iq_mean from_ms=%d to_ms=%d value=%.4f\n", fromMs, toMs, *mean);
    EXPECT_GE(*mean, low) << "q-axis current between " << fromMs << " and " << toMs << " ms";
    EXPECT_LE(*mean, high) << "q-axis current between " << fromMs << " and " << toMs << " ms";
}

THEN(R"(the rotor shall travel at least {float} rad after the last setpoint)", (float minimum))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    const auto travel = TravelAfterSetpoint(setup);
    ASSERT_TRUE(travel.has_value()) << "No plant samples after the last setpoint";
    std::fprintf(stderr, "[METRIC] sil travel_rad value=%.3f\n", *travel);
    EXPECT_GE(*travel, minimum) << "The rotor travelled " << *travel << " rad";
}

THEN(R"(the rotor shall travel less than {float} rad after the last setpoint)", (float maximum))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    const auto travel = TravelAfterSetpoint(setup);
    ASSERT_TRUE(travel.has_value()) << "No plant samples after the last setpoint";
    std::fprintf(stderr, "[METRIC] sil travel_rad value=%.3f\n", *travel);
    EXPECT_LT(*travel, maximum) << "The rotor travelled " << *travel << " rad";
}

THEN(R"(the rotor shall arrive within {float} rad of the last setpoint between {float} and {float} ms after it)", (float band, float earliestMs, float latestMs))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });
    const auto stamp = SetpointStampAfterEnable(setup);
    ASSERT_TRUE(stamp.has_value()) << "No setpoint was applied after enable";

    std::optional<uint32_t> arrival;
    for (const auto& sample : setup.trace.Samples())
    {
        const bool inside = sample.tick >= *stamp && std::abs(sample.thetaMech - setup.lastSetpoint->value) <= band;
        if (!inside)
            arrival.reset();
        else if (!arrival)
            arrival = sample.tick;
    }

    ASSERT_TRUE(arrival.has_value()) << "The rotor did not stay within " << band << " rad of the setpoint";
    const float arrivalMs = static_cast<float>(*arrival - *stamp) / static_cast<float>(setup.plant.baseFrequencyHz) * kMilliPerSecond;
    std::fprintf(stderr, "[METRIC] sil position_arrival band_rad=%.4f arrival_ms=%.3f\n", band, arrivalMs);
    EXPECT_GE(arrivalMs, earliestMs);
    EXPECT_LE(arrivalMs, latestMs);
}

THEN(R"(the rotor shall follow a trapezoidal profile of {float} rad\/s and {float} rad\/s2 within {float} rad through the cruise)", (float velocity, float acceleration, float limit))
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });
    const auto stamp = SetpointStampAfterEnable(setup);
    ASSERT_TRUE(stamp.has_value()) << "No setpoint was applied after enable";
    const auto start = PositionAtTick(setup, *stamp);
    ASSERT_TRUE(start.has_value()) << "No plant samples after the last setpoint";

    constexpr float kLatestStartMs = 3.0f;
    constexpr float kOffsetStepMs = 0.25f;
    const float rampMs = velocity / acceleration * kMilliPerSecond;
    float best{ std::numeric_limits<float>::max() };
    float bestOffsetMs{ 0.0f };
    for (float offsetMs = 0.0f; offsetMs <= kLatestStartMs; offsetMs += kOffsetStepMs)
    {
        const float error = ProfileFollowingError(setup, *stamp, *start, setup.lastSetpoint->value, { velocity, acceleration }, offsetMs, 1.1f * rampMs, 1.4f * rampMs);
        if (error < best)
        {
            best = error;
            bestOffsetMs = offsetMs;
        }
    }

    std::fprintf(stderr, "[METRIC] sil position_profile cruise_error_rad=%.4f start_offset_ms=%.2f\n", best, bestOffsetMs);
    EXPECT_LT(best, limit);
}

THEN(R"(the response shall have no dropped samples)")
{
    auto& setup = context.Get<ScenarioSetup>();
    FeedRecord(setup, std::chrono::milliseconds{ 0 });

    EXPECT_EQ(setup.trace.Dropped(), 0u) << "The recorder dropped samples";
    EXPECT_EQ(setup.trace.Gaps(), 0u) << "The captured samples are not uniformly spaced";
}
