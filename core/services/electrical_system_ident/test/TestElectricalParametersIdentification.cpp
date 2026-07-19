#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    float MechanicalAngle(std::size_t stepIndex, std::size_t totalSteps, std::size_t expectedPolePairs)
    {
        constexpr std::size_t stepsPerRevolution = 12;
        auto electricalRevolutions = totalSteps / stepsPerRevolution;
        auto electricalAngle = (static_cast<float>(stepIndex) / static_cast<float>(totalSteps)) * (static_cast<float>(electricalRevolutions) * twoPi);
        return electricalAngle / static_cast<float>(expectedPolePairs);
    }

    class ElectricalParametersIdentificationTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr float vdcValue = 24.0f;
        static constexpr float maxCurrent = 3.0f;
        static constexpr float samplingFrequency = 10000.0f;
        static constexpr std::size_t injectionFrequency = 250;
        static constexpr std::size_t injectionVoltagePercent = 15;
        static constexpr std::size_t warmupPeriods = 10;
        static constexpr std::size_t measurementPeriods = 50;
        static constexpr std::size_t voltageToCurrentDelaySamples = 1;

        // Phase-to-midpoint amplitude for a center-aligned half-bridge is modIndex * Vdc / 2.
        // The alpha modulation index equals injectionVoltagePercent / 100.
        static constexpr float voltsPerModulation = vdcValue / 2.0f;
        static constexpr float injectionAmplitude = static_cast<float>(injectionVoltagePercent) / 100.0f * voltsPerModulation;
        static constexpr float omega = twoPi * static_cast<float>(injectionFrequency);
        static constexpr float samplingPeriod = 1.0f / samplingFrequency;
        static constexpr std::size_t samplesPerPeriod = static_cast<std::size_t>(samplingFrequency) / injectionFrequency;

        std::size_t encoderStepIndex = 0;

        StrictMock<foc::FieldOrientedControllerInterfaceMock> driverMock;
        StrictMock<foc::EncoderMock> encoderMock;
        foc::Volts vdc{ vdcValue };
        foc::Clarke clarke;
        services::ElectricalParametersIdentificationImpl identification{ driverMock, encoderMock, vdc };

        services::ElectricalParametersIdentification::ResistanceAndInductanceConfig DefaultConfig() const
        {
            services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
            config.injectionFrequency = hal::Hertz{ injectionFrequency };
            config.injectionVoltagePercent = hal::Percent{ injectionVoltagePercent };
            config.warmupPeriods = warmupPeriods;
            config.measurementPeriods = measurementPeriods;
            config.voltageToCurrentDelaySamples = voltageToCurrentDelaySamples;
            return config;
        }

        // Feed the analytic AC steady-state current i_alpha[k] = I*sin(applied_phase[k - delay] - phi),
        // inverse-Clarke'd to (Ia, Ib, Ic), for the full warmup + measurement window. The current at
        // sample k is produced by the applied-voltage phase from `delay` samples earlier, modelling the
        // one-sample PWM->ADC pipeline lag the demodulation compensates for. Before the burst starts the
        // applied voltage is zero, so the first `delay` samples carry no injected current. An optional
        // low-frequency back-EMF disturbance current can be superimposed to test demod rejection.
        void FeedHfBurst(float resistance, float inductance, float backEmfCurrentAmplitude = 0.0f, float backEmfFrequency = 0.0f, std::size_t delaySamples = voltageToCurrentDelaySamples)
        {
            const float impedance = std::sqrt(resistance * resistance + (omega * inductance) * (omega * inductance));
            const float current = injectionAmplitude / impedance;
            const float phi = std::atan2(omega * inductance, resistance);
            const float backEmfOmega = twoPi * backEmfFrequency;

            const std::size_t totalSamples = (warmupPeriods + measurementPeriods) * samplesPerPeriod;
            for (std::size_t k = 0; k < totalSamples; ++k)
            {
                float iAlpha = 0.0f;
                if (k >= delaySamples)
                {
                    const float appliedPhase = static_cast<float>(k - delaySamples) * omega * samplingPeriod;
                    iAlpha = current * std::sin(appliedPhase - phi);
                }

                if (backEmfCurrentAmplitude != 0.0f)
                    iAlpha += backEmfCurrentAmplitude * std::sin(backEmfOmega * static_cast<float>(k) * samplingPeriod);

                const auto phases = clarke.Inverse(foc::TwoPhase{ iAlpha, 0.0f });
                driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ phases.a }, foc::Ampere{ phases.b }, foc::Ampere{ phases.c } });
            }
        }
    };
}

TEST_F(ElectricalParametersIdentificationTest, arms_phase_currents_before_pwm_output_after_stop)
{
    Sequence seq;
    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, Stop())
        .InSequence(seq);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _))
        .InSequence(seq)
        .WillOnce([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_))
        .Times(AnyNumber())
        .InSequence(seq);

    identification.EstimateResistanceAndInductance(DefaultConfig(), [](auto) {});
}

TEST_F(ElectricalParametersIdentificationTest, phase_currents_callback_is_inert_after_completion)
{
    const float trueR = 1.5f;
    const float trueLs = 0.002f;
    int completions = 0;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateResistanceAndInductance(DefaultConfig(), [&](auto)
        {
            ++completions;
        });

    FeedHfBurst(trueR, trueLs);

    ASSERT_EQ(completions, 1);

    for (std::size_t i = 0; i < samplesPerPeriod * 4; ++i)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 100.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_EQ(completions, 1);
}

TEST_F(ElectricalParametersIdentificationTest, estimates_resistance_and_inductance_accurately)
{
    const float trueR = 1.5f;
    const float trueLs = 0.002f;

    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateResistanceAndInductance(DefaultConfig(), [&](auto r)
        {
            result = r;
        });

    FeedHfBurst(trueR, trueLs);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->resistance.Value(), trueR, trueR * 0.05f);
    EXPECT_NEAR(result->inductance.Value(), trueLs * 1000.0f, trueLs * 1000.0f * 0.10f);
    EXPECT_NEAR(result->inverterVoltageOffset.Value(), 0.0f, 1e-6f);
    EXPECT_LT(result->fitQuality, 0.05f);
}

TEST_F(ElectricalParametersIdentificationTest, rejects_low_frequency_back_emf_disturbance)
{
    const float trueR = 1.5f;
    const float trueLs = 0.002f;
    const float backEmfAmplitude = 0.5f;
    const float backEmfFrequency = 2.0f;

    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateResistanceAndInductance(DefaultConfig(), [&](auto r)
        {
            result = r;
        });

    FeedHfBurst(trueR, trueLs, backEmfAmplitude, backEmfFrequency);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->resistance.Value(), trueR, trueR * 0.05f);
    EXPECT_NEAR(result->inductance.Value(), trueLs * 1000.0f, trueLs * 1000.0f * 0.10f);
}

TEST_F(ElectricalParametersIdentificationTest, applies_delta_winding_correction)
{
    const float terminalR = 1.0f;
    const float terminalLs = 0.001f;

    auto config = DefaultConfig();
    config.windingConfig = services::WindingConfiguration::Delta;
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateResistanceAndInductance(config, [&](auto r)
        {
            result = r;
        });

    FeedHfBurst(terminalR, terminalLs);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->resistance.Value(), terminalR * 1.5f, terminalR * 1.5f * 0.05f);
    EXPECT_NEAR(result->inductance.Value(), terminalLs * 1000.0f * 1.5f, terminalLs * 1000.0f * 1.5f * 0.10f);
}

TEST_F(ElectricalParametersIdentificationTest, returns_nullopt_when_current_is_below_floor)
{
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;
    bool completed = false;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateResistanceAndInductance(DefaultConfig(), [&](auto r)
        {
            completed = true;
            result = r;
        });

    const std::size_t totalSamples = (warmupPeriods + measurementPeriods) * samplesPerPeriod;
    for (std::size_t k = 0; k < totalSamples; ++k)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(completed);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, aborts_once_with_nullopt_when_peak_current_exceeds_max)
{
    // A very low-impedance motor draws a steady current whose peak exceeds MaxCurrentSupported.
    const float lowR = 0.05f;
    const float lowLs = 0.00002f;

    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;
    int completions = 0;

    EXPECT_CALL(driverMock, MaxCurrentSupported())
        .WillRepeatedly(Return(foc::Ampere{ maxCurrent }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(AnyNumber());
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateResistanceAndInductance(DefaultConfig(), [&](auto r)
        {
            ++completions;
            result = r;
        });

    FeedHfBurst(lowR, lowLs);

    EXPECT_EQ(completions, 1);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, returns_nullopt_when_injection_frequency_is_zero)
{
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;
    bool completed = false;

    auto config = DefaultConfig();
    config.injectionFrequency = hal::Hertz{ 0 };

    identification.EstimateResistanceAndInductance(config, [&](auto r)
        {
            completed = true;
            result = r;
        });

    ASSERT_TRUE(completed);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, returns_nullopt_when_injection_frequency_does_not_divide_sampling)
{
    std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result;
    bool completed = false;

    auto config = DefaultConfig();
    config.injectionFrequency = hal::Hertz{ 333 };

    identification.EstimateResistanceAndInductance(config, [&](auto r)
        {
            completed = true;
            result = r;
        });

    ASSERT_TRUE(completed);
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
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

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
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

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
    EXPECT_CALL(driverMock, Stop()).Times(AnyNumber());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}
