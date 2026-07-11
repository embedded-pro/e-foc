#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    MATCHER_P(PhasePwmDutyCyclesEq, expected, "")
    {
        return arg.a.Value() == expected.a.Value() &&
               arg.b.Value() == expected.b.Value() &&
               arg.c.Value() == expected.c.Value();
    }

    float SimulateRLModelCurrent(float voltage, float resistance, float inductance, float time)
    {
        return (voltage / resistance) * (1.0f - std::exp(-time / (inductance / resistance)));
    }

    float MechanicalAngle(std::size_t stepIndex, std::size_t totalSteps, std::size_t expectedPolePairs)
    {
        constexpr std::size_t stepsPerRevolution = 12;
        auto electricalRevolutions = totalSteps / stepsPerRevolution;
        auto electricalAngle = (static_cast<float>(stepIndex) / static_cast<float>(totalSteps)) * (static_cast<float>(electricalRevolutions) * 2.0f * std::numbers::pi_v<float>);
        return electricalAngle / static_cast<float>(expectedPolePairs);
    }

    class ElectricalParametersIdentificationTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr float vdcValue = 24.0f;
        static constexpr float maxCurrent = 5.0f;
        static constexpr float probeVoltage = 5.0f / 100.0f * vdcValue;
        static constexpr std::size_t probeBufferSize = 512;
        static constexpr std::size_t steadyStateSamples = 32;
        static constexpr std::size_t numLevels = 3;
        static constexpr float samplingPeriod = 0.0001f;

        std::size_t encoderStepIndex = 0;

        StrictMock<foc::FieldOrientedControllerInterfaceMock> driverMock;
        StrictMock<foc::EncoderMock> encoderMock;
        foc::Volts vdc{ vdcValue };
        services::ElectricalParametersIdentificationImpl identification{ driverMock, encoderMock, vdc };

        void FeedProbeTransient(float resistance, float inductance)
        {
            for (std::size_t i = 0; i < probeBufferSize; ++i)
            {
                float t = static_cast<float>(i) * samplingPeriod;
                float current = SimulateRLModelCurrent(probeVoltage, resistance, inductance, t);
                driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ current }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
            }
        }

        void FeedLevelSteadyState(float iSs)
        {
            ForwardTime(std::chrono::milliseconds{ 300 });
            for (std::size_t s = 0; s < steadyStateSamples; ++s)
                driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ iSs }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
        }

        float ComputeLevelDuty(float rCoarse, float targetFraction) const
        {
            constexpr float neutralDuty = 1.0f;
            float rawDuty = targetFraction * maxCurrent * rCoarse / vdcValue * 100.0f + neutralDuty;
            return std::clamp(rawDuty, 5.0f, 80.0f);
        }

        float ComputeLevelVoltage(float duty) const
        {
            return (duty - 1.0f) / 100.0f * vdcValue;
        }
    };
}

TEST_F(ElectricalParametersIdentificationTest, probe_step_sets_probe_duty_and_starts_collecting_immediately)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(
        foc::PhasePwmDutyCycles{ hal::Percent{ 5 }, hal::Percent{ 1 }, hal::Percent{ 1 } })));

    identification.EstimateResistanceAndInductance(config, [](auto) {});
}

TEST_F(ElectricalParametersIdentificationTest, estimates_resistance_and_inductance_accurately)
{
    const float trueR = 1.5f;
    const float trueL = 0.002f;
    const std::array<float, numLevels> fractions{ 0.3f, 0.5f, 0.7f };

    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(4);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateResistanceAndInductance(config, [&](auto r) { result = r; });

    FeedProbeTransient(trueR, trueL);

    const float rCoarse = trueR;
    for (std::size_t j = 0; j < numLevels; ++j)
    {
        float duty = ComputeLevelDuty(rCoarse, fractions[j]);
        float vj = ComputeLevelVoltage(duty);
        FeedLevelSteadyState(vj / trueR);
    }

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->resistance.Value(), trueR, trueR * 0.05f);
    EXPECT_NEAR(result->inductance.Value(), trueL * 1000.0f, trueL * 1000.0f * 0.10f);
    EXPECT_NEAR(result->inverterVoltageOffset.Value(), 0.0f, 0.05f);
    EXPECT_LT(result->fitQuality, 0.05f);
}

TEST_F(ElectricalParametersIdentificationTest, r_fit_cancels_constant_inverter_voltage_offset)
{
    const float trueR = 1.5f;
    const float trueL = 0.002f;
    const float vOffset = 0.3f;
    const std::array<float, numLevels> fractions{ 0.3f, 0.5f, 0.7f };

    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(4);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateResistanceAndInductance(config, [&](auto r) { result = r; });

    FeedProbeTransient(trueR, trueL);

    const float rCoarse = trueR;
    for (std::size_t j = 0; j < numLevels; ++j)
    {
        float duty = ComputeLevelDuty(rCoarse, fractions[j]);
        float vj = ComputeLevelVoltage(duty);
        FeedLevelSteadyState((vj - vOffset) / trueR);
    }

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->resistance.Value(), trueR, trueR * 0.05f);
    EXPECT_NEAR(result->inverterVoltageOffset.Value(), vOffset, 0.1f);
}

TEST_F(ElectricalParametersIdentificationTest, applies_delta_winding_correction)
{
    const float terminalR = 1.0f;
    const float trueL = 0.001f;
    const std::array<float, numLevels> fractions{ 0.3f, 0.5f, 0.7f };

    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
    config.windingConfig = services::WindingConfiguration::Delta;
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(4);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateResistanceAndInductance(config, [&](auto r) { result = r; });

    FeedProbeTransient(terminalR, trueL);

    const float rCoarse = terminalR;
    for (std::size_t j = 0; j < numLevels; ++j)
    {
        float duty = ComputeLevelDuty(rCoarse, fractions[j]);
        float vj = ComputeLevelVoltage(duty);
        FeedLevelSteadyState(vj / terminalR);
    }

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->resistance.Value(), terminalR * 1.5f, terminalR * 1.5f * 0.05f);
}

TEST_F(ElectricalParametersIdentificationTest, returns_nullopt_when_probe_current_is_zero)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateResistanceAndInductance(config, [&](auto r) { result = r; });

    for (std::size_t i = 0; i < probeBufferSize; ++i)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_FALSE(result.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, returns_nullopt_when_level_current_is_zero)
{
    const float trueR = 1.5f;
    const float trueL = 0.002f;
    const std::array<float, numLevels> fractions{ 0.3f, 0.5f, 0.7f };

    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateResistanceAndInductance(config, [&](auto r) { result = r; });

    FeedProbeTransient(trueR, trueL);

    const float rCoarse = trueR;
    FeedLevelSteadyState(ComputeLevelVoltage(ComputeLevelDuty(rCoarse, fractions[0])) / trueR);
    FeedLevelSteadyState(ComputeLevelVoltage(ComputeLevelDuty(rCoarse, fractions[1])) / trueR);
    FeedLevelSteadyState(0.0f);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_initializes_encoder_and_applies_voltages)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));

    identification.EstimateNumberOfPolePairs(config, [](auto) {});
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_calculates_correct_pole_pairs_for_4_pole_motor)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 5 * 12;
    constexpr std::size_t expectedPolePairs = 2;

    encoderStepIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.0f }))
        .WillRepeatedly([this, totalSteps, expectedPolePairs]()
            {
                ++encoderStepIndex;
                return foc::Radians{ MechanicalAngle(encoderStepIndex, totalSteps, expectedPolePairs) };
            });
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result) { resultPolePairs = result; });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_calculates_correct_pole_pairs_for_6_pole_motor)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 5 * 12;
    constexpr std::size_t expectedPolePairs = 3;

    encoderStepIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.0f }))
        .WillRepeatedly([this, totalSteps, expectedPolePairs]()
            {
                ++encoderStepIndex;
                return foc::Radians{ MechanicalAngle(encoderStepIndex, totalSteps, expectedPolePairs) };
            });
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result) { resultPolePairs = result; });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}
