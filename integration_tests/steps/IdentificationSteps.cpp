#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

using namespace integration;

namespace
{
    constexpr auto kIdentificationTimeout = std::chrono::seconds{ 180 };
    constexpr auto kTraceTimeout = std::chrono::seconds{ 15 };
    constexpr auto kExcitationWallClockTimeout = std::chrono::seconds{ 300 };
    constexpr float kMilliPerUnit = 1000.0f;
    constexpr float kMicroPerUnit = 1000000.0f;
    constexpr float kReferenceCelsius = 25.0f;
    constexpr std::size_t kElectricalResponseBytes = 5;
    constexpr useconds_t kStatePollIntervalUs = 100000;

    hal::Can::Id ResponseId(uint8_t messageType)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, can::focMotorCategoryId, messageType, Fixture::kServerNodeId));
    }

    float Fixed16(const hal::Can::Message& payload, std::size_t offset, int32_t scale)
    {
        const auto raw = static_cast<int16_t>((static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1]);
        return static_cast<float>(raw) / static_cast<float>(scale);
    }

    hal::Can::Message EncodeFixed16BE(float value, int32_t scale)
    {
        services::CanPayloadWriter payload;
        payload.WriteFixed16(value, scale);
        return payload.Message();
    }

    std::optional<float> ValueAfter(const std::string& line, const std::string& key)
    {
        const auto at = line.find(key);
        if (at == std::string::npos)
            return std::nullopt;
        return std::strtof(line.c_str() + at + key.size(), nullptr);
    }

    std::optional<std::string> LastLineWith(std::size_t fromLine, const std::string& needle)
    {
        const auto& lines = TargetInteractor::Instance().SerialLines();
        std::optional<std::string> found;
        for (std::size_t i = fromLine; i < lines.size(); ++i)
            if (lines[i].find(needle) != std::string::npos)
                found = lines[i];
        return found;
    }

    template<typename Predicate>
    bool DrainUntil(Fixture& fixture, Predicate done, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (done())
                return true;
            fixture.DrainLines(std::chrono::milliseconds{ 50 });
        }
        return done();
    }

    // Both the calibration record and the [EST] lines carry key=value pairs; a line only sets the keys it has.
    void ParseTraceValues(const std::string& line, IdentifiedParameters& out)
    {
        if (const auto r = ValueAfter(line, " R="))
            out.resistanceOhm = r;
        if (const auto l = ValueAfter(line, " L_mH="))
            out.inductanceMilliHenry = l;
        if (const auto p = ValueAfter(line, " p="))
            out.polePairs = static_cast<unsigned>(std::lround(*p));
        if (const auto j = ValueAfter(line, " J_uNms2="))
            out.inertiaKgM2 = *j / kMicroPerUnit;
        if (const auto b = ValueAfter(line, " B_uNms="))
            out.frictionNmSPerRad = *b / kMicroPerUnit;
    }

    std::optional<float> PlantTruth(const ScenarioSetup& setup, const std::string& quantity, float windingCelsius)
    {
        const auto& plant = setup.plant;
        if (quantity == "resistance")
            return plant.statorResistanceOhm * (1.0f + plant.copperTempCoeff * (windingCelsius - kReferenceCelsius));
        if (quantity == "inductance")
            return plant.dAxisInductanceHenry * kMilliPerUnit;
        if (quantity == "inertia")
            return plant.rotorInertiaKgM2;
        if (quantity == "friction")
            return plant.viscousDampingNmSPerRad;
        return std::nullopt;
    }

    std::optional<float> Estimate(const IdentifiedParameters& parameters, const std::string& quantity)
    {
        if (quantity == "resistance")
            return parameters.resistanceOhm;
        if (quantity == "inductance")
            return parameters.inductanceMilliHenry;
        if (quantity == "inertia")
            return parameters.inertiaKgM2;
        if (quantity == "friction")
            return parameters.frictionNmSPerRad;
        return std::nullopt;
    }

    void CompareWithPlant(const ScenarioSetup& setup, const IdentifiedParameters& parameters, const char* source, const std::string& quantity, float limitPercent, float windingCelsius)
    {
        const auto truth = PlantTruth(setup, quantity, windingCelsius);
        ASSERT_TRUE(truth.has_value()) << "Unknown quantity: " << quantity;
        ASSERT_NE(*truth, 0.0f) << "The plant's " << quantity << " is zero; a relative error is undefined";

        const auto value = Estimate(parameters, quantity);
        ASSERT_TRUE(value.has_value()) << "No " << source << " " << quantity << " was captured";

        const float errorPercent = std::fabs(*value - *truth) / std::fabs(*truth) * 100.0f;
        std::fprintf(stderr, "[METRIC] ident %s %s %s plant=%.6g value=%.6g error_pct=%.2f\n",
            source, quantity.c_str(), setup.plantName.c_str(), *truth, *value, errorPercent);

        EXPECT_LE(errorPercent, limitPercent) << source << " " << quantity << " is " << *value << " against a plant of " << *truth;
    }

    void FeedTrace(ScenarioSetup& setup)
    {
        const auto& lines = TargetInteractor::Instance().SerialLines();
        for (; setup.consumedLines < lines.size(); ++setup.consumedLines)
            setup.trace.Consume(lines[setup.consumedLines]);
    }

    // Guest time, not wall-clock: the plant's own samples say how far its clock has advanced, which is
    // what makes the excitation the same length on every host.
    bool WaitForGuestTicks(Fixture& fixture, ScenarioSetup& setup, uint32_t ticks)
    {
        FeedTrace(setup);
        const auto from = setup.trace.LastSampleTick();
        if (!from)
            return false;

        return DrainUntil(fixture, [&]
            {
                FeedTrace(setup);
                const auto last = setup.trace.LastSampleTick();
                return last && *last >= *from + ticks;
            },
            kExcitationWallClockTimeout);
    }
}

WHEN(R"(electrical identification is run)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    const auto mark = fixture.CapturedLineCount();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId, {}, kIdentificationTimeout))
        << "Electrical identification did not complete successfully";

    const auto payload = fixture.FindCapturedCanFrame(ResponseId(can::focElectricalParamsResponseId), mark);
    ASSERT_TRUE(payload.has_value()) << "The target sent no electrical parameters response";
    ASSERT_GE(payload->size(), kElectricalResponseBytes) << "Electrical parameters response is too short";

    setup.identified.resistanceOhm = Fixed16(*payload, 0, can::focResistanceScale);
    setup.identified.inductanceMilliHenry = Fixed16(*payload, 2, can::focInductanceScale);
    setup.identified.polePairs = (*payload)[4];
}

WHEN(R"(the full calibration is run from the terminal)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    const auto mark = fixture.CapturedLineCount();
    ASSERT_TRUE(fixture.SendCommand("calibrate")) << "Could not send the calibrate command";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::calibrating, std::chrono::seconds{ 15 }))
        << "Calibration never started";

    std::optional<can::FocMotorState> state;
    const auto deadline = std::chrono::steady_clock::now() + kIdentificationTimeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        state = fixture.ReadMotorState();
        if (state && *state != can::FocMotorState::calibrating)
            break;
        usleep(kStatePollIntervalUs);
    }

    ASSERT_TRUE(state.has_value()) << "No telemetry while waiting for the calibration to end";
    ASSERT_EQ(*state, can::FocMotorState::idle) << "Calibration ended in state " << static_cast<int>(*state) << " instead of Ready";

    std::optional<std::string> record;
    ASSERT_TRUE(DrainUntil(fixture, [&]
        {
            record = LastLineWith(mark, "[SM] Calibration record:");
            return record.has_value();
        },
        kTraceTimeout))
        << "The target never traced the calibration record it stored";

    ParseTraceValues(*record, setup.identified);
}

WHEN(R"(the speed setpoint alternates between {float} and {float} rad\/s every {int} ms for {int} ms)", (float low, float high, int dwellMs, int totalMs))
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_NE(setup.plant.responseSampleRateHz, 0u) << "Excitation is paced on the plant's clock: record the plant response first";

    const auto ticksPerMs = setup.plant.baseFrequencyHz / 1000u;
    const auto dwellTicks = static_cast<uint32_t>(dwellMs) * ticksPerMs;
    const auto totalTicks = static_cast<uint32_t>(totalMs) * ticksPerMs;

    FeedTrace(setup);
    const auto start = setup.trace.LastSampleTick();
    ASSERT_TRUE(start.has_value()) << "No plant samples yet: is the motor enabled and the response recorded?";

    bool atHigh = false;
    while (true)
    {
        FeedTrace(setup);
        const auto now = setup.trace.LastSampleTick().value_or(*start);
        if (now >= *start + totalTicks)
            break;

        atHigh = !atHigh;
        const float target = atHigh ? high : low;
        ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetSpeedSetpointId, EncodeFixed16BE(target, can::focSpeedScale)))
            << "Speed setpoint command rejected";
        setup.lastSetpoint = Setpoint{ can::focSetSpeedSetpointId, target };

        ASSERT_TRUE(WaitForGuestTicks(fixture, setup, dwellTicks)) << "The plant clock stopped advancing during the excitation";
    }
}

WHEN(R"(the online estimates are read)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    const auto mark = fixture.CapturedLineCount();
    ASSERT_TRUE(fixture.SendCommand("estimate_status")) << "Could not send the estimate_status command";

    std::optional<std::string> mechanical;
    std::optional<std::string> electrical;
    ASSERT_TRUE(DrainUntil(fixture, [&]
        {
            mechanical = LastLineWith(mark, "[EST] Mech:");
            electrical = LastLineWith(mark, "[EST] Elec:");
            return mechanical.has_value() && electrical.has_value();
        },
        kTraceTimeout))
        << "The target did not print its online estimates";

    ParseTraceValues(*mechanical, setup.online);
    ParseTraceValues(*electrical, setup.online);
}

THEN(R"(the identified {word} shall be within {float} % of the plant)", (std::string quantity, float limitPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareWithPlant(setup, setup.identified, "identified", quantity, limitPercent, kReferenceCelsius);
}

THEN(R"(the identified pole pairs shall match the plant)")
{
    const auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(setup.identified.polePairs.has_value()) << "No identified pole-pair count was captured";

    std::fprintf(stderr, "[METRIC] ident identified polePairs %s plant=%u value=%u\n",
        setup.plantName.c_str(), static_cast<unsigned>(setup.plant.polePairs), *setup.identified.polePairs);

    EXPECT_EQ(*setup.identified.polePairs, static_cast<unsigned>(setup.plant.polePairs));
}

THEN(R"(the online {word} estimate shall be within {float} % of the plant)", (std::string quantity, float limitPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareWithPlant(setup, setup.online, "online", quantity, limitPercent, kReferenceCelsius);
}

THEN(R"(the online {word} estimate shall be within {float} % of the plant at {float} celsius)", (std::string quantity, float limitPercent, float windingCelsius))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareWithPlant(setup, setup.online, "online", quantity, limitPercent, windingCelsius);
}
