#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
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
    constexpr auto kRecordPollSlice = std::chrono::milliseconds{ 50 };
    constexpr auto kExcitationWallClockTimeout = std::chrono::seconds{ 300 };
    constexpr float kMilliPerUnit = 1000.0f;
    constexpr float kReferenceCelsius = 25.0f;

    hal::Can::Message EncodeFloat32(float value)
    {
        services::CanPayloadWriter payload;
        payload.WriteFloat32(value);
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
        out.fluxLinkageWeber = ReadParameterValue(fixture, can::FocParameter::fluxLinkage);
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
        if (quantity == "fluxLinkage")
            return plant.fluxLinkageWeber;
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
        if (quantity == "fluxLinkage")
            return parameters.fluxLinkageWeber;
        return std::nullopt;
    }

    std::optional<float> StoredValue(const services::CalibrationData& record, const std::string& quantity)
    {
        if (quantity == "resistance")
            return record.rPhase;
        if (quantity == "inductance")
            return record.lD;
        if (quantity == "inertia")
            return record.inertia;
        if (quantity == "friction")
            return record.frictionViscous;
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

    void FeedTrace(ScenarioSetup& setup, std::chrono::milliseconds timeout = std::chrono::milliseconds{ 0 })
    {
        for (const auto& line : TargetInteractor::Instance().TakePlantRecord(timeout))
            setup.trace.Consume(line);
    }

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

    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId, {}, kIdentificationTimeout))
        << "Electrical identification did not complete successfully";

    ReadCalibrationRecord(fixture, setup.identified);
    const auto polePairs = ReadParameterValue(fixture, can::FocParameter::polePairs);
    ASSERT_TRUE(setup.identified.resistanceOhm.has_value() && setup.identified.inductanceMilliHenry.has_value() && polePairs.has_value())
        << "The target did not report the identified parameters over CAN";
    setup.identified.polePairs = static_cast<unsigned>(*polePairs);
}

WHEN(R"(the calibration record is read)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    ReadCalibrationRecord(fixture, setup.identified);
    ASSERT_TRUE(setup.identified.resistanceOhm.has_value()) << "The target did not report its calibration record over CAN";
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
            ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSetSpeedSetpointId, EncodeFloat32(target)))
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

WHEN(R"(the drive is recalibrated over CAN)")
{
    ASSERT_TRUE(context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focRecalibrateId))
        << "The target refused the Recalibrate command";
}

WHEN(R"(the model is reset over CAN)")
{
    ASSERT_TRUE(context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focResetModelId))
        << "The target refused the Reset Model command";
}

THEN(R"(a recalibration over CAN shall be refused)")
{
    EXPECT_FALSE(context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focRecalibrateId))
        << "The target accepted the Recalibrate command";
}

WHEN(R"(the model in use is read)")
{
    auto& fixture = context.Get<Fixture>();
    auto& setup = context.Get<ScenarioSetup>();

    setup.inUse.resistanceOhm = ReadParameterValue(fixture, can::FocParameter::modelResistance);
    setup.inUse.inductanceMilliHenry = ReadParameterValue(fixture, can::FocParameter::modelInductance);
    setup.inUse.inertiaKgM2 = ReadParameterValue(fixture, can::FocParameter::modelInertia);
    setup.inUse.frictionNmSPerRad = ReadParameterValue(fixture, can::FocParameter::modelViscousFriction);

    ASSERT_TRUE(setup.inUse.resistanceOhm.has_value() && setup.inUse.inductanceMilliHenry.has_value() &&
                setup.inUse.inertiaKgM2.has_value() && setup.inUse.frictionNmSPerRad.has_value())
        << "The target did not report the model in use over CAN";
}

THEN(R"(the {word} in use shall be within {float} % of the plant)", (std::string quantity, float limitPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareWithPlant(setup, setup.inUse, "in use", quantity, limitPercent, kReferenceCelsius);
}

THEN(R"(the {word} in use shall be within {float} % of the stored calibration)", (std::string quantity, float limitPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    const auto stored = StoredValue(setup.nvm.calibration, quantity);
    ASSERT_TRUE(stored.has_value() && *stored != 0.0f) << "No stored calibration value for " << quantity;

    const auto value = Estimate(setup.inUse, quantity);
    ASSERT_TRUE(value.has_value()) << "No " << quantity << " in use was captured";

    const float errorPercent = std::fabs(*value - *stored) / std::fabs(*stored) * 100.0f;
    std::fprintf(stderr, "[METRIC] ident in use %s against the stored calibration %s stored=%.6g value=%.6g error_pct=%.2f\n",
        quantity.c_str(), setup.plantName.c_str(), *stored, *value, errorPercent);
    EXPECT_LE(errorPercent, limitPercent) << quantity << " in use is " << *value << " against a stored " << *stored;
}

THEN(R"(the identified {word} shall be within {float} % of the plant)", (std::string quantity, float limitPercent))
{
    const auto& setup = context.Get<ScenarioSetup>();
    CompareWithPlant(setup, setup.identified, "identified", quantity, limitPercent, kReferenceCelsius);
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
