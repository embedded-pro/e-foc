#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

using namespace integration;

namespace
{
    constexpr auto kIdentificationTimeout = std::chrono::seconds{ 180 };
    constexpr auto kResponseTimeout = std::chrono::seconds{ 15 };
    constexpr auto kRecordPollSlice = std::chrono::milliseconds{ 50 };
    constexpr auto kExcitationWallClockTimeout = std::chrono::seconds{ 300 };
    constexpr float kMilliPerUnit = 1000.0f;
    constexpr float kReferenceCelsius = 25.0f;
    constexpr std::size_t kElectricalResponseBytes = 5;

    hal::Can::Id ResponseId(const Fixture& fixture, uint8_t messageType)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, can::focMotorCategoryId, messageType, fixture.ServerNodeId()));
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

    void ReadCalibrationRecord(Fixture& fixture, IdentifiedParameters& out)
    {
        out.resistanceOhm = ReadParameterValue(fixture, can::FocParameter::resistance);
        out.inductanceMilliHenry = ReadParameterValue(fixture, can::FocParameter::inductance);
        if (const auto polePairs = ReadParameterValue(fixture, can::FocParameter::polePairs))
            out.polePairs = static_cast<unsigned>(std::lround(*polePairs));
        out.inertiaKgM2 = ReadParameterValue(fixture, can::FocParameter::inertia);
        out.frictionNmSPerRad = ReadParameterValue(fixture, can::FocParameter::viscousFriction);
        out.currentOffsetAmpereA = ReadParameterValue(fixture, can::FocParameter::currentOffsetA);
        out.currentOffsetAmpereB = ReadParameterValue(fixture, can::FocParameter::currentOffsetB);
        out.currentOffsetAmpereC = ReadParameterValue(fixture, can::FocParameter::currentOffsetC);
        out.coggingPeakNm = ReadParameterValue(fixture, can::FocParameter::coggingPeak);
    }

    void CompareOffsetWithPlant(const ScenarioSetup& setup, const char* phase, std::optional<float> identified, float bias, float limitMilliAmpere)
    {
        ASSERT_TRUE(identified.has_value()) << "No current offset was captured for phase " << phase;

        const float errorMilliAmpere = std::fabs(*identified - bias) * kMilliPerUnit;
        std::fprintf(stderr, "[METRIC] ident identified currentOffset%s %s plant_mA=%.3f value_mA=%.3f error_mA=%.3f\n",
            phase, setup.plantName.c_str(), bias * kMilliPerUnit, *identified * kMilliPerUnit, errorMilliAmpere);

        EXPECT_LE(errorMilliAmpere, limitMilliAmpere) << "phase " << phase << " offset is " << *identified << " A against a bias of " << bias << " A";
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
        if (quantity == "cogging")
            return plant.coggingTorqueNm;
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
        if (quantity == "cogging")
            return parameters.coggingPeakNm;
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

    void ExpectAbovePlant(const ScenarioSetup& setup, const IdentifiedParameters& parameters, const char* source, const std::string& quantity, float minimumPercent)
    {
        const auto truth = PlantTruth(setup, quantity, kReferenceCelsius);
        ASSERT_TRUE(truth.has_value()) << "Unknown quantity: " << quantity;
        ASSERT_NE(*truth, 0.0f) << "The plant's " << quantity << " is zero; a relative error is undefined";

        const auto value = Estimate(parameters, quantity);
        ASSERT_TRUE(value.has_value()) << "No " << source << " " << quantity << " was captured";

        const float excessPercent = (*value - *truth) / std::fabs(*truth) * 100.0f;
        std::fprintf(stderr, "[METRIC] ident %s %s %s plant=%.6g value=%.6g excess_pct=%.2f\n",
            source, quantity.c_str(), setup.plantName.c_str(), *truth, *value, excessPercent);

        EXPECT_GE(excessPercent, minimumPercent) << source << " " << quantity << " is " << *value << " against a plant of " << *truth;
    }

    void FeedTrace(ScenarioSetup& setup, std::chrono::milliseconds timeout = std::chrono::milliseconds{ 0 })
    {
        for (const auto& line : TargetInteractor::Instance().TakePlantRecord(timeout))
            setup.trace.Consume(line);
    }

    // Guest time, not wall-clock: the plant's own samples say how far its clock has advanced, which is
    // what makes the excitation the same length on every host.
    bool WaitForGuestTicks(ScenarioSetup& setup, uint32_t ticks)
    {
        FeedTrace(setup);
        const auto from = setup.trace.LastSampleTick();
        if (!from)
            return false;

        const auto deadline = std::chrono::steady_clock::now() + kExcitationWallClockTimeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (const auto last = setup.trace.LastSampleTick(); last && *last >= *from + ticks)
                return true;
            FeedTrace(setup, kRecordPollSlice);
        }
        return false;
    }
}

WHEN(R"(electrical identification is run)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    const auto mark = fixture.MarkCapture();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId, {}, kIdentificationTimeout))
        << "Electrical identification did not complete successfully";

    const auto payload = fixture.WaitForCapturedCanFrame(ResponseId(fixture, can::focElectricalParamsResponseId), mark, kResponseTimeout);
    ASSERT_TRUE(payload.has_value()) << "The target sent no electrical parameters response";
    ASSERT_GE(payload->size(), kElectricalResponseBytes) << "Electrical parameters response is too short";

    setup.identified.resistanceOhm = Fixed16(*payload, 0, can::focResistanceScale);
    setup.identified.inductanceMilliHenry = Fixed16(*payload, 2, can::focInductanceScale);
    setup.identified.polePairs = (*payload)[4];
}

WHEN(R"(the full calibration is run)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focCalibrateId, {}, kIdentificationTimeout))
        << "The calibration did not complete";

    ReadCalibrationRecord(fixture, setup.identified);
    ASSERT_TRUE(setup.identified.resistanceOhm.has_value()) << "The target did not report its calibration record over CAN";
}

namespace
{
    void AlternateSpeedSetpoint(Fixture& fixture, ScenarioSetup& setup, float low, float high, int dwellMs, int totalMs)
    {
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
            setup.lastSetpoint = Setpoint{ can::focSetSpeedSetpointId, target, fixture.ServerNodeId() };

            ASSERT_TRUE(WaitForGuestTicks(setup, dwellTicks)) << "The plant clock stopped advancing during the excitation";
        }
    }
}

WHEN(R"(the speed setpoint alternates between {float} and {float} rad\/s every {int} ms for {int} ms)", (float low, float high, int dwellMs, int totalMs))
{
    AlternateSpeedSetpoint(context.Get<Fixture>(), context.Get<ScenarioSetup>(), low, high, dwellMs, totalMs);
}

WHEN(R"(the speed setpoint is held at {float} rad\/s for {int} ms)", (float speed, int durationMs))
{
    AlternateSpeedSetpoint(context.Get<Fixture>(), context.Get<ScenarioSetup>(), speed, speed, durationMs, durationMs);
}

WHEN(R"(the online estimates are read)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    setup.online.resistanceOhm = ReadParameterValue(fixture, can::FocParameter::estimatedResistance);
    setup.online.inductanceMilliHenry = ReadParameterValue(fixture, can::FocParameter::estimatedInductance);
    setup.online.inertiaKgM2 = ReadParameterValue(fixture, can::FocParameter::estimatedInertia);
    setup.online.frictionNmSPerRad = ReadParameterValue(fixture, can::FocParameter::estimatedViscousFriction);

    ASSERT_TRUE(setup.online.resistanceOhm.has_value() && setup.online.inductanceMilliHenry.has_value() &&
                setup.online.inertiaKgM2.has_value() && setup.online.frictionNmSPerRad.has_value())
        << "The target did not report its online estimates over CAN";
}

THEN(R"(the identified {word} shall be within {float} % of the plant)", (std::string quantity, float limitPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareWithPlant(setup, setup.identified, "identified", quantity, limitPercent, kReferenceCelsius);
}

THEN(R"(the calibration shall store no cogging map)")
{
    const auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(setup.identified.coggingPeakNm.has_value()) << "No calibration record was captured";

    std::fprintf(stderr, "[METRIC] ident identified cogging %s plant=%.6g value=%.6g\n",
        setup.plantName.c_str(), setup.plant.coggingTorqueNm, *setup.identified.coggingPeakNm);

    EXPECT_EQ(*setup.identified.coggingPeakNm, 0.0f) << "A cogging map with a peak of " << *setup.identified.coggingPeakNm << " N·m was stored";
}

THEN(R"(the identified current offsets shall be within {float} mA of the plant)", (float limitMilliAmpere))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareOffsetWithPlant(setup, "A", setup.identified.currentOffsetAmpereA, setup.plant.noiseBiasAmpereA, limitMilliAmpere);
    CompareOffsetWithPlant(setup, "B", setup.identified.currentOffsetAmpereB, setup.plant.noiseBiasAmpereB, limitMilliAmpere);
    CompareOffsetWithPlant(setup, "C", setup.identified.currentOffsetAmpereC, setup.plant.noiseBiasAmpereC, limitMilliAmpere);
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

THEN(R"(the online {word} estimate shall read at least {float} % above the plant)", (std::string quantity, float minimumPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    ExpectAbovePlant(setup, setup.online, "online", quantity, minimumPercent);
}

THEN(R"(the identified {word} shall read at least {float} % above the plant)", (std::string quantity, float minimumPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    ExpectAbovePlant(setup, setup.identified, "identified", quantity, minimumPercent);
}
