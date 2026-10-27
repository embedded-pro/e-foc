#pragma once

#include "core/services/cogging_compensation/CoggingMap.hpp"
#include "integration_tests/model_in_the_loop/ModelInTheLoop.hpp"
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <optional>

namespace mil
{
    enum class Quantity
    {
        resistance,
        inductance,
        inertia,
        friction,
        cogging
    };

    constexpr float referenceCelsius = 25.0f;
    constexpr float milliPerUnit = 1000.0f;

    inline const char* QuantityName(Quantity quantity)
    {
        switch (quantity)
        {
            case Quantity::resistance:
                return "resistance";
            case Quantity::inductance:
                return "inductance";
            case Quantity::inertia:
                return "inertia";
            case Quantity::friction:
                return "friction";
            case Quantity::cogging:
                return "cogging";
        }
        return "?";
    }

    inline float PlantTruth(const sil::SilPlantConfig& plant, Quantity quantity, float windingCelsius = referenceCelsius)
    {
        switch (quantity)
        {
            case Quantity::resistance:
                return plant.statorResistanceOhm * (1.0f + plant.copperTempCoeff * (windingCelsius - referenceCelsius));
            case Quantity::inductance:
                return plant.dAxisInductanceHenry * milliPerUnit;
            case Quantity::inertia:
                return plant.rotorInertiaKgM2;
            case Quantity::friction:
                return plant.viscousDampingNmSPerRad;
            case Quantity::cogging:
                return plant.coggingTorqueNm;
        }
        return 0.0f;
    }

    inline float Identified(const services::CalibrationData& record, Quantity quantity)
    {
        switch (quantity)
        {
            case Quantity::resistance:
                return record.rPhase;
            case Quantity::inductance:
                return record.lD;
            case Quantity::inertia:
                return record.inertia;
            case Quantity::friction:
                return record.frictionViscous;
            case Quantity::cogging:
                return services::CoggingPeak(record.coggingTorque, record.coggingTorqueScale).Value();
        }
        return 0.0f;
    }

    inline float Online(const state_machine::OnlineEstimates& estimates, Quantity quantity)
    {
        switch (quantity)
        {
            case Quantity::resistance:
                return estimates.resistance.Value();
            case Quantity::inductance:
                return estimates.inductance.Value();
            case Quantity::inertia:
                return estimates.inertia.Value();
            case Quantity::friction:
                return estimates.viscousFriction.Value();
            case Quantity::cogging:
                return 0.0f;
        }
        return 0.0f;
    }

    inline void ExpectWithin(const Scenario& scenario, const char* source, Quantity quantity, float value, float limitPercent, float windingCelsius = referenceCelsius)
    {
        const float truth = PlantTruth(scenario.plant, quantity, windingCelsius);
        ASSERT_NE(truth, 0.0f);

        const float errorPercent = std::fabs(value - truth) / std::fabs(truth) * 100.0f;
        std::fprintf(stderr, "[METRIC] mil ident %s %s %s plant=%.6g value=%.6g error_pct=%.2f\n",
            source, QuantityName(quantity), scenario.plantName.c_str(), truth, value, errorPercent);

        EXPECT_LE(errorPercent, limitPercent) << source << " " << QuantityName(quantity) << " is " << value << " against a plant of " << truth;
    }

    inline void ExpectAbove(const Scenario& scenario, const char* source, Quantity quantity, float value, float minimumPercent)
    {
        const float truth = PlantTruth(scenario.plant, quantity);
        ASSERT_NE(truth, 0.0f);

        const float excessPercent = (value - truth) / std::fabs(truth) * 100.0f;
        std::fprintf(stderr, "[METRIC] mil ident %s %s %s plant=%.6g value=%.6g excess_pct=%.2f\n",
            source, QuantityName(quantity), scenario.plantName.c_str(), truth, value, excessPercent);

        EXPECT_GE(excessPercent, minimumPercent) << source << " " << QuantityName(quantity) << " is " << value << " against a plant of " << truth;
    }

    inline void ExpectOffsetsWithin(const Scenario& scenario, const services::CalibrationData& record, float limitMilliAmpere)
    {
        EXPECT_LE(std::fabs(record.currentOffsetA - scenario.plant.noiseBiasAmpereA) * milliPerUnit, limitMilliAmpere);
        EXPECT_LE(std::fabs(record.currentOffsetB - scenario.plant.noiseBiasAmpereB) * milliPerUnit, limitMilliAmpere);
        EXPECT_LE(std::fabs(record.currentOffsetC - scenario.plant.noiseBiasAmpereC) * milliPerUnit, limitMilliAmpere);
    }

    inline void ExpectStepEnvelope(const std::optional<integration::response::StepResponseMetrics>& metrics, float settleMs, float overshootPercent, float riseMs, float tailPercent, float steadyStateError)
    {
        ASSERT_TRUE(metrics.has_value());
        EXPECT_LE(metrics->settlingTimeS * milliPerUnit, settleMs);
        EXPECT_LT(metrics->percentOvershoot, overshootPercent);
        EXPECT_LE(metrics->riseTimeS * milliPerUnit, riseMs);
        EXPECT_LE(metrics->tailBandPercent, tailPercent);
        EXPECT_LT(std::fabs(metrics->steadyStateError), steadyStateError);
    }

    inline void ExpectNoDroppedSamples(ModelInTheLoop& mil)
    {
        EXPECT_EQ(mil.Trace().Dropped(), 0u);
        EXPECT_EQ(mil.Trace().Gaps(), 0u);
    }

    inline void ExpectIdentified(ModelInTheLoop& mil, Quantity quantity, float limitPercent)
    {
        ExpectWithin(mil.Setup(), "identified", quantity, Identified(mil.CalibrationRecord(), quantity), limitPercent);
    }

    inline void ExpectPolePairsMatch(ModelInTheLoop& mil)
    {
        EXPECT_EQ(mil.CalibrationRecord().polePairs, mil.Setup().plant.polePairs);
    }

    inline void ExpectOnline(ModelInTheLoop& mil, Quantity quantity, float limitPercent, float windingCelsius = referenceCelsius)
    {
        const auto estimates = mil.OnlineEstimates();
        ASSERT_TRUE(estimates.has_value());
        ExpectWithin(mil.Setup(), "online", quantity, Online(*estimates, quantity), limitPercent, windingCelsius);
    }

    struct StepRow
    {
        const char* label;
        float target;
        float bandPercent;
        float settleMs;
        float overshootPercent;
        float riseMs;
        float tailPercent;
        float steadyStateError;
    };

    struct Loop
    {
        integration::response::Signal signal;
        state_machine::ControlMode mode;
        uint32_t sampleRateHz;
        uint32_t maxSamplesFromRest;
        float captureMs;
        float initial;
    };

    constexpr Loop speedLoop{ integration::response::Signal::speed, state_machine::ControlMode::speed, 1000, 800, 550.0f, 20.0f };
    constexpr Loop positionLoop{ integration::response::Signal::position, state_machine::ControlMode::position, 1000, 800, 550.0f, 1.5f };
    constexpr Loop currentLoop{ integration::response::Signal::currentQ, state_machine::ControlMode::torque, 20000, 400, 15.0f, 0.5f };

    inline bool Apply(ModelInTheLoop& mil, integration::response::Signal signal, float setpoint)
    {
        switch (signal)
        {
            case integration::response::Signal::speed:
                return mil.SetSpeed(setpoint);
            case integration::response::Signal::position:
                return mil.SetPosition(setpoint);
            case integration::response::Signal::currentQ:
                return mil.SetTorque(setpoint);
        }
        return false;
    }

    inline bool RunsSelectedAlgorithm(ModelInTheLoop& mil, integration::response::Signal signal)
    {
        const auto& config = mil.Setup().nvm.config;
        switch (signal)
        {
            case integration::response::Signal::speed:
                return static_cast<uint8_t>(mil.ControlMode().ActiveSpeedAlgorithm()) == config.speedAlgorithm;
            case integration::response::Signal::position:
                return static_cast<uint8_t>(mil.ControlMode().ActivePositionAlgorithm()) == config.positionAlgorithm;
            case integration::response::Signal::currentQ:
                return static_cast<uint8_t>(mil.ControlMode().ActiveCurrentAlgorithm()) == config.currentAlgorithm;
        }
        return false;
    }

    inline void StepFromRest(Scenario scenario, const Loop& loop, const StepRow& row)
    {
        SCOPED_TRACE(row.label);
        scenario.Recorded(loop.sampleRateHz, loop.maxSamplesFromRest).Calibrated().BootsIn(loop.mode);

        ModelInTheLoop mil{ scenario };
        ASSERT_TRUE(mil.Align());
        EXPECT_TRUE(RunsSelectedAlgorithm(mil, loop.signal));
        ASSERT_TRUE(Apply(mil, loop.signal, row.target));
        ASSERT_TRUE(mil.Enable());
        ASSERT_TRUE(mil.CaptureAfterEnable(loop.captureMs));

        ExpectStepEnvelope(MeasureStep(mil, loop.signal, row.bandPercent, row.label), row.settleMs, row.overshootPercent, row.riseMs, row.tailPercent, row.steadyStateError);
        ExpectNoDroppedSamples(mil);
        EXPECT_TRUE(mil.Disable());
    }
}
