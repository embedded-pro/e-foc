#include "cucumber_cpp/Steps.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "integration_tests/support/ControllerSelection.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>
#include <optional>
#include <thread>

using namespace integration;

namespace
{
    constexpr float kMicroPerUnit = 1.0e6f;
    constexpr auto kPollInterval = std::chrono::milliseconds{ 20 };

    float WrappedDifference(float a, float b)
    {
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        float d = std::fmod(a - b, twoPi);
        if (d > std::numbers::pi_v<float>)
            d -= twoPi;
        else if (d < -std::numbers::pi_v<float>)
            d += twoPi;
        return d;
    }
}

WHEN(R"(the {word} loop algorithm is set to {word})", (std::string loopName, std::string algorithmName))
{
    auto& fixture = context.Get<Fixture>();
    const auto loop = LoopNamed(loopName);
    ASSERT_TRUE(loop.has_value()) << "Unknown control loop: " << loopName;
    const auto algorithm = AlgorithmNamed(*loop, algorithmName);
    ASSERT_TRUE(algorithm.has_value()) << "Unknown " << loopName << " loop algorithm: " << algorithmName;
    ASSERT_TRUE(SelectAlgorithmOverCan(fixture, *loop, *algorithm))
        << "The target refused the " << algorithmName << " " << loopName << " loop algorithm";
}

THEN(R"(the identified pole pairs shall be {int})", (int expected))
{
    const auto& identified = context.Get<ScenarioSetup>().identified;
    ASSERT_TRUE(identified.polePairs.has_value()) << "No pole pairs were identified";
    EXPECT_EQ(static_cast<int>(*identified.polePairs), expected);
}

THEN(R"(the identified resistance shall be between {float} and {float} ohm)", (float low, float high))
{
    const auto& identified = context.Get<ScenarioSetup>().identified;
    ASSERT_TRUE(identified.resistanceOhm.has_value()) << "No resistance was identified";
    EXPECT_GE(*identified.resistanceOhm, low);
    EXPECT_LE(*identified.resistanceOhm, high);
}

THEN(R"(the identified inductance shall be between {float} and {float} mH)", (float low, float high))
{
    const auto& identified = context.Get<ScenarioSetup>().identified;
    ASSERT_TRUE(identified.inductanceMilliHenry.has_value()) << "No inductance was identified";
    EXPECT_GE(*identified.inductanceMilliHenry, low);
    EXPECT_LE(*identified.inductanceMilliHenry, high);
}

THEN(R"(the identified inertia shall be between {float} and {float} uNms2)", (float low, float high))
{
    const auto& identified = context.Get<ScenarioSetup>().identified;
    ASSERT_TRUE(identified.inertiaKgM2.has_value()) << "No inertia was identified";
    EXPECT_GE(*identified.inertiaKgM2 * kMicroPerUnit, low);
    EXPECT_LE(*identified.inertiaKgM2 * kMicroPerUnit, high);
}

THEN(R"(the measured speed shall settle at {float} rad\/s within {float} rad\/s in {int} ms)", (float expected, float tolerance, int timeoutMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ timeoutMs };
    float last = 0.0f;
    int consecutive = 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto speed = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(speed.has_value()) << "No speed telemetry";
        last = *speed;
        consecutive = std::fabs(last - expected) <= tolerance ? consecutive + 1 : 0;
        if (consecutive >= 3)
            return;
        std::this_thread::sleep_for(kPollInterval);
    }

    FAIL() << "The speed did not settle at " << expected << " rad/s within " << tolerance << " rad/s; last " << last;
}

THEN(R"(the measured position shall settle at {float} rad within {float} rad in {int} ms)", (float expected, float tolerance, int timeoutMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ timeoutMs };
    float last = 0.0f;
    int consecutive = 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto position = fixture.ReadMeasuredPosition();
        ASSERT_TRUE(position.has_value()) << "No position telemetry";
        last = *position;
        consecutive = std::fabs(WrappedDifference(last, expected)) <= tolerance ? consecutive + 1 : 0;
        if (consecutive >= 3)
            return;
        std::this_thread::sleep_for(kPollInterval);
    }

    FAIL() << "The position did not settle at " << expected << " rad within " << tolerance << " rad; last " << last;
}

THEN(R"(the measured position shall hold at {float} rad within {float} rad for {int} ms)", (float expected, float tolerance, int durationMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ durationMs };
    float worst = 0.0f;
    float worstAt = expected;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto position = fixture.ReadMeasuredPosition();
        ASSERT_TRUE(position.has_value()) << "No position telemetry";
        const float error = std::fabs(WrappedDifference(*position, expected));
        if (error > worst)
        {
            worst = error;
            worstAt = *position;
        }
        std::this_thread::sleep_for(kPollInterval);
    }

    EXPECT_LE(worst, tolerance) << "The position left " << expected << " rad by " << worst << " rad, reaching " << worstAt;
}

THEN(R"(the reported position shall read {float} rad within {float} rad without wrapping)", (float expected, float tolerance))
{
    auto& fixture = context.Get<Fixture>();
    const auto position = fixture.ReadMeasuredPosition();
    ASSERT_TRUE(position.has_value()) << "No position telemetry";
    EXPECT_NEAR(*position, expected, tolerance) << "The reported position is not in the setpoint's turn";
}

THEN(R"(the measured speed shall exceed {float} rad\/s within {int} ms)", (float threshold, int timeoutMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ timeoutMs };

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto speed = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(speed.has_value()) << "No speed telemetry";
        if (threshold >= 0.0f ? *speed >= threshold : *speed <= threshold)
            return;
        std::this_thread::sleep_for(kPollInterval);
    }

    FAIL() << "The rotor never reached " << threshold << " rad/s";
}

THEN(R"(the motor shall not report a fault)")
{
    auto& fixture = context.Get<Fixture>();
    const auto state = fixture.ReadMotorState();
    ASSERT_TRUE(state.has_value()) << "No state telemetry";
    EXPECT_NE(*state, can::FocMotorState::fault);
}

THEN(R"(the measured speed shall stay below {float} rad\/s in magnitude for {int} ms)", (float limit, int durationMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ durationMs };
    float peak = 0.0f;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto speed = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(speed.has_value()) << "No speed telemetry";
        peak = std::max(peak, std::fabs(*speed));
    }

    std::printf("[METRIC] hil peak_speed_rad_s value=%.1f limit=%.1f\n", static_cast<double>(peak), static_cast<double>(limit));
    EXPECT_LT(peak, limit) << "The rotor reached " << peak << " rad/s";
}

THEN(R"(the measured speed shall fall below {float} rad\/s between {int} and {int} ms after the quick stop)", (float speed, int fromMs, int toMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(setup.quickStopAcknowledgedAt.has_value()) << "No quick stop was issued";
    const auto issued = *setup.quickStopAcknowledgedAt;
    const auto deadline = issued + std::chrono::milliseconds{ 2 * toMs };
    std::optional<double> fellAfterMs;

    while (!fellAfterMs && std::chrono::steady_clock::now() < deadline)
    {
        const auto measured = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(measured.has_value()) << "No speed telemetry";
        if (std::fabs(*measured) < speed)
            fellAfterMs = std::chrono::duration<double, std::milli>{ std::chrono::steady_clock::now() - issued }.count();
    }

    ASSERT_TRUE(fellAfterMs.has_value()) << "The speed stayed above " << speed << " rad/s for " << 2 * toMs << " ms after the quick stop";
    std::printf("[METRIC] hil quick_stop_below_speed_ms value=%.1f\n", *fellAfterMs);
    EXPECT_GE(*fellAfterMs, fromMs);
    EXPECT_LE(*fellAfterMs, toMs);
}
