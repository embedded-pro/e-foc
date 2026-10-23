#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>
#include <optional>
#include <string>
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

    std::string CommandFor(const std::string& loop)
    {
        if (loop == "current")
            return "select_current_algorithm";
        if (loop == "speed")
            return "select_speed_algorithm";
        return "select_position_algorithm";
    }

    float Beyond(float initial, float expected, float value)
    {
        const float excess = expected >= initial ? value - expected : expected - value;
        return std::fmax(excess, 0.0f);
    }

    long long MillisecondsSince(std::chrono::steady_clock::time_point start)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    }

    struct Hold
    {
        int samples{ 0 };
        double sum{ 0.0 };
        double sumOfSquares{ 0.0 };
        float lowest{ 0.0f };
        float highest{ 0.0f };
        float worst{ 0.0f };

        void Add(float error)
        {
            lowest = samples == 0 ? error : std::fmin(lowest, error);
            highest = samples == 0 ? error : std::fmax(highest, error);
            worst = std::fmax(worst, std::fabs(error));
            sum += error;
            sumOfSquares += static_cast<double>(error) * error;
            ++samples;
        }

        double Mean() const
        {
            return sum / samples;
        }

        double Deviation() const
        {
            return std::sqrt(std::fmax(sumOfSquares / samples - Mean() * Mean(), 0.0));
        }
    };

    bool SetTorque(Fixture& fixture, float iq)
    {
        char command[32];
        std::snprintf(command, sizeof(command), "set_torque %.3f", iq);
        return fixture.SendCommand(command);
    }

    std::optional<float> WaitUntilStill(Fixture& fixture)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
        std::optional<float> previous;
        int still = 0;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto position = fixture.ReadMeasuredPosition();
            if (!position.has_value())
                return std::nullopt;
            still = previous.has_value() && std::fabs(WrappedDifference(*position, *previous)) < 0.005f ? still + 1 : 0;
            if (still >= 10)
                return position;
            previous = position;
            std::this_thread::sleep_for(kPollInterval);
        }

        return std::nullopt;
    }

    template<class Read, class Error>
    Hold Sample(Read read, Error error, int durationMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ durationMs };
        Hold hold;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto value = read();
            if (!value.has_value())
                break;
            hold.Add(error(*value));
            std::this_thread::sleep_for(kPollInterval);
        }

        return hold;
    }
}

WHEN(R"(the {word} loop algorithm is set to {word})", (std::string loop, std::string algorithm))
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand(CommandFor(loop) + " " + algorithm)) << "Could not send the algorithm selection";
    ASSERT_NE(fixture.lastResponse.find("selected"), std::string::npos) << "The " << loop << " algorithm " << algorithm << " was not accepted: " << fixture.lastResponse;
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
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds{ timeoutMs };
    std::optional<float> initial;
    float last = 0.0f;
    float overshoot = 0.0f;
    long long enteredMs = 0;
    int consecutive = 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto speed = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(speed.has_value()) << "No speed telemetry";
        last = *speed;
        if (!initial)
            initial = last;
        overshoot = std::fmax(overshoot, Beyond(*initial, expected, last));
        if (std::fabs(last - expected) <= tolerance)
        {
            if (consecutive == 0)
                enteredMs = MillisecondsSince(start);
            ++consecutive;
        }
        else
            consecutive = 0;
        if (consecutive >= 3)
        {
            std::fprintf(stderr, "[METRIC] hil speed settle target=%.3f from=%.3f settle_ms=%lld overshoot=%.3f last=%.3f\n",
                expected, *initial, enteredMs, overshoot, last);
            return;
        }
        std::this_thread::sleep_for(kPollInterval);
    }

    FAIL() << "The speed did not settle at " << expected << " rad/s within " << tolerance << " rad/s; last " << last;
}

THEN(R"(the measured position shall settle at {float} rad within {float} rad in {int} ms)", (float expected, float tolerance, int timeoutMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds{ timeoutMs };
    std::optional<float> initial;
    float last = 0.0f;
    float overshoot = 0.0f;
    long long enteredMs = 0;
    int consecutive = 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto position = fixture.ReadMeasuredPosition();
        ASSERT_TRUE(position.has_value()) << "No position telemetry";
        last = *position;
        const float error = WrappedDifference(last, expected);
        if (!initial)
            initial = error;
        overshoot = std::fmax(overshoot, Beyond(*initial, 0.0f, error));
        if (std::fabs(error) <= tolerance)
        {
            if (consecutive == 0)
                enteredMs = MillisecondsSince(start);
            ++consecutive;
        }
        else
            consecutive = 0;
        if (consecutive >= 3)
        {
            std::fprintf(stderr, "[METRIC] hil position settle target=%.3f from_error=%.3f settle_ms=%lld overshoot=%.3f last=%.3f\n",
                expected, *initial, enteredMs, overshoot, last);
            return;
        }
        std::this_thread::sleep_for(kPollInterval);
    }

    FAIL() << "The position did not settle at " << expected << " rad within " << tolerance << " rad; last " << last;
}

THEN(R"(the measured speed shall exceed {float} rad\/s within {int} ms)", (float threshold, int timeoutMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds{ timeoutMs };

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto speed = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(speed.has_value()) << "No speed telemetry";
        if (threshold >= 0.0f ? *speed >= threshold : *speed <= threshold)
        {
            std::fprintf(stderr, "[METRIC] hil torque reach threshold=%.3f reach_ms=%lld speed=%.3f\n", threshold, MillisecondsSince(start), *speed);
            return;
        }
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

THEN(R"(the measured speed shall hold at {float} rad\/s within {float} rad\/s for {int} ms)", (float expected, float tolerance, int durationMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto hold = Sample([&fixture]()
        {
            return fixture.ReadMeasuredSpeed();
        },
        [expected](float speed)
        {
            return speed - expected;
        },
        durationMs);

    ASSERT_GT(hold.samples, 0) << "No speed telemetry";
    std::fprintf(stderr, "[METRIC] hil speed hold target=%.3f n=%d mean_err=%.3f std=%.3f min_err=%.3f max_err=%.3f\n",
        expected, hold.samples, hold.Mean(), hold.Deviation(), hold.lowest, hold.highest);
    EXPECT_LE(hold.worst, tolerance) << "The speed left " << expected << " rad/s by " << hold.worst << " rad/s";
}

THEN(R"(the measured position shall hold at {float} rad within {float} rad for {int} ms)", (float expected, float tolerance, int durationMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto hold = Sample([&fixture]()
        {
            return fixture.ReadMeasuredPosition();
        },
        [expected](float position)
        {
            return WrappedDifference(position, expected);
        },
        durationMs);

    ASSERT_GT(hold.samples, 0) << "No position telemetry";
    std::fprintf(stderr, "[METRIC] hil position hold target=%.3f n=%d mean_err=%.4f std=%.4f min_err=%.4f max_err=%.4f\n",
        expected, hold.samples, hold.Mean(), hold.Deviation(), hold.lowest, hold.highest);
    EXPECT_LE(hold.worst, tolerance) << "The position left " << expected << " rad by " << hold.worst << " rad";
}

THEN(R"(the current that turns the rotor {float} rad is mapped over {int} trials in steps of {float} A up to {float} A held for {int} ms)", (float breakawayDisplacement, int trials, float step, float limit, int heldMs))
{
    auto& fixture = context.Get<Fixture>();

    for (int trial = 0; trial < trials; ++trial)
    {
        const float direction = trial % 2 == 0 ? 1.0f : -1.0f;
        ASSERT_TRUE(SetTorque(fixture, 0.0f)) << "set_torque rejected";
        const auto start = WaitUntilStill(fixture);
        ASSERT_TRUE(start.has_value()) << "The rotor did not come to rest";

        float iq = 0.0f;
        float creep = 0.0f;
        bool broke = false;
        for (int level = 1; static_cast<float>(level) * step <= limit + 1.0e-6f && !broke; ++level)
        {
            iq = direction * static_cast<float>(level) * step;
            ASSERT_TRUE(SetTorque(fixture, iq)) << "set_torque rejected";
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ heldMs };
            while (std::chrono::steady_clock::now() < deadline)
            {
                const auto position = fixture.ReadMeasuredPosition();
                ASSERT_TRUE(position.has_value()) << "No position telemetry";
                const float displacement = WrappedDifference(*position, *start);
                if (std::fabs(displacement) >= breakawayDisplacement)
                {
                    broke = true;
                    break;
                }
                creep = displacement;
                std::this_thread::sleep_for(kPollInterval);
            }
        }

        ASSERT_TRUE(SetTorque(fixture, 0.0f)) << "set_torque rejected";
        std::fprintf(stderr, "[METRIC] hil breakaway trial=%d dir=%+.0f start=%.3f iq=%.3f creep=%.3f broke=%d\n",
            trial, direction, *start, iq, creep, broke ? 1 : 0);
    }
}

THEN(R"(the measured position and speed are traced for {int} ms)", (int durationMs))
{
    auto& fixture = context.Get<Fixture>();
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds{ durationMs };

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto position = fixture.ReadMeasuredPosition();
        const auto speed = fixture.ReadMeasuredSpeed();
        ASSERT_TRUE(position.has_value() && speed.has_value()) << "No telemetry";
        std::fprintf(stderr, "[TRACE] t=%lld pos=%.2f speed=%.0f\n", MillisecondsSince(start), *position, *speed);
        std::this_thread::sleep_for(kPollInterval);
    }
}

THEN(R"(the rest positions are surveyed over {int} pulses of {float} A for {int} ms)", (int pulses, float iq, int pulseMs))
{
    auto& fixture = context.Get<Fixture>();

    for (int pulse = 0; pulse < pulses; ++pulse)
    {
        ASSERT_TRUE(SetTorque(fixture, 0.0f)) << "set_torque rejected";
        const auto rest = WaitUntilStill(fixture);
        ASSERT_TRUE(rest.has_value()) << "The rotor did not come to rest";
        std::fprintf(stderr, "[METRIC] hil rest pulse=%d iq=%.3f pos=%.3f\n", pulse, iq, *rest);

        ASSERT_TRUE(SetTorque(fixture, iq)) << "set_torque rejected";
        std::this_thread::sleep_for(std::chrono::milliseconds{ pulseMs });
    }

    ASSERT_TRUE(SetTorque(fixture, 0.0f)) << "set_torque rejected";
}
