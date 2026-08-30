#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/electrical_system_ident/SinusoidalInductanceEstimator.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr float fs = 10000.0f;
    constexpr float vdc = 24.0f;
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    // Drives a ZOH alpha-axis RL plant with the same sinusoidal voltage the estimator injects,
    // feeding back phase-A current (= alpha current for this excitation).
    // Returns the result AND the ZOH-theoretic expected L in mH so the caller can assert
    // algorithm correctness without relying on continuous-time approximation accuracy.
    struct PlantSimResult
    {
        services::SinusoidalInductanceEstimator::Result result;
        float expectedInductanceMH{ 0.0f };
    };

    PlantSimResult RunPlantSimulation(
        StrictMock<drivers::ThreePhaseInverterMock>& driverMock,
        services::SinusoidalInductanceEstimator& estimator,
        const services::SinusoidalInductanceEstimator::Config& config,
        float rTerminal,
        float lTerminalMilliHenry)
    {
        foc::MotorModelParameters plantParams{};
        plantParams.resistance = foc::Ohm{ rTerminal };
        plantParams.inductance = foc::MilliHenry{ lTerminalMilliHenry };
        plantParams.busVoltage = foc::Volts{ vdc };
        plantParams.samplingFrequency = hal::Hertz{ static_cast<std::size_t>(fs) };
        const auto plant = foc::CurrentPlantModel::FromParameters(plantParams);

        // The impl rounds f_inj to the nearest integer samples/period and uses that exact frequency.
        // Mirror this here so the test voltage matches what the impl injects.
        const float fInj = static_cast<float>(config.injectionFrequency.Value());
        const auto samplesPerPeriod = static_cast<std::size_t>(std::round(fs / fInj));
        const float omegaExact = twoPi * fs / static_cast<float>(samplesPerPeriod);
        const float phaseInc = omegaExact / fs;  // = 2π / samplesPerPeriod

        const float vNorm = static_cast<float>(config.injectionVoltagePercent.Value()) / 100.0f;
        const float vTermAmp = vNorm * 0.75f * vdc;
        const auto totalSamples = (config.warmupPeriods + config.measurementPeriods) * samplesPerPeriod;

        // ZOH-theoretic expected L: Im(Z_ZOH) / (omegaExact * terminalFactor).
        // Z_ZOH = (1 − ad·e^{−jω·Ts}) / bd  →  Im(Z_ZOH) = ad·sin(ω·Ts) / bd.
        const float omegaTs = phaseInc;  // = omegaExact / fs
        const float imZZOH = plant.ad * std::sin(omegaTs) / plant.bd;
        const float termFactor = config.windingConfig == services::WindingConfiguration::Delta ? 0.5f : 1.5f;
        const float expectedL = imZZOH / (omegaExact * termFactor) * 1000.0f;  // mH

        services::SinusoidalInductanceEstimator::Result result{};

        EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _))
            .WillOnce([&driverMock](auto, const auto& cb)
                {
                    driverMock.StorePhaseCurrentsCallback(cb);
                });
        EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(totalSamples);
        EXPECT_CALL(driverMock, Stop());

        estimator.Start(config, [&result](auto r) { result = r; });

        float phase = 0.0f;
        float iAlpha = 0.0f;

        for (std::size_t k = 0; k < totalSamples; ++k)
        {
            driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
                foc::Ampere{ iAlpha }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

            const float vTerm = vTermAmp * std::sin(phase);
            phase += phaseInc;
            if (phase >= twoPi)
                phase -= twoPi;

            iAlpha = plant.ad * iAlpha + plant.bd * vTerm;
        }

        return { result, expectedL };
    }

    class SinusoidalInductanceEstimatorTest : public ::testing::Test
    {
    public:
        StrictMock<drivers::ThreePhaseInverterMock> driverMock;
        foc::Volts busVoltage{ vdc };
        services::SinusoidalInductanceEstimator estimator{ driverMock, busVoltage };
    };
}

TEST_F(SinusoidalInductanceEstimatorTest, start_registers_phase_current_callback)
{
    services::SinusoidalInductanceEstimator::Config config{};

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _));

    estimator.Start(config, [](auto) {});
}

TEST_F(SinusoidalInductanceEstimatorTest, zero_current_response_returns_no_inductance)
{
    services::SinusoidalInductanceEstimator::Config config{
        hal::Hertz{ 700 }, hal::Percent{ 15 }, 2, 5, 1, services::WindingConfiguration::Wye
    };

    const float fInj = static_cast<float>(config.injectionFrequency.Value());
    const auto samplesPerPeriod = static_cast<std::size_t>(std::round(fs / fInj));
    const auto totalSamples = (config.warmupPeriods + config.measurementPeriods) * samplesPerPeriod;

    services::SinusoidalInductanceEstimator::Result result{};

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(totalSamples);
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [&result](auto r) { result = r; });

    for (std::size_t k = 0; k < totalSamples; ++k)
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_FALSE(result.inductance.has_value());
    EXPECT_FLOAT_EQ(result.fitQuality, 0.0f);
}

TEST_F(SinusoidalInductanceEstimatorTest, recovers_inductance_within_1_percent_of_zoh_for_standard_motor)
{
    // Terminal parameters for a wye motor: R_terminal = 1.5 * R_phase, L_terminal = 1.5 * L_phase.
    const float rTerminal = 0.75f;
    const float lTerminalMH = 0.75f;

    services::SinusoidalInductanceEstimator::Config config{
        hal::Hertz{ 700 }, hal::Percent{ 15 }, 5, 20, 1, services::WindingConfiguration::Wye
    };

    auto [result, expectedL] = RunPlantSimulation(driverMock, estimator, config, rTerminal, lTerminalMH);

    ASSERT_TRUE(result.inductance.has_value());
    EXPECT_NEAR(result.inductance->Value(), expectedL, expectedL * 0.01f);
    EXPECT_GT(result.fitQuality, 0.9f);
}

TEST_F(SinusoidalInductanceEstimatorTest, recovers_inductance_within_1_percent_of_zoh_for_jk42bls01)
{
    // JK42BLS01 terminal values (wye): R_phase=0.073Ω → R_terminal=0.1095Ω; L_phase=0.5mH → L_terminal=0.75mH.
    const float rTerminal = 0.073f * 1.5f;
    const float lTerminalMH = 0.5f * 1.5f;

    services::SinusoidalInductanceEstimator::Config config{
        hal::Hertz{ 700 }, hal::Percent{ 15 }, 5, 20, 1, services::WindingConfiguration::Wye
    };

    auto [result, expectedL] = RunPlantSimulation(driverMock, estimator, config, rTerminal, lTerminalMH);

    ASSERT_TRUE(result.inductance.has_value());
    EXPECT_NEAR(result.inductance->Value(), expectedL, expectedL * 0.01f);
    EXPECT_GT(result.fitQuality, 0.9f);
}

TEST_F(SinusoidalInductanceEstimatorTest, recovers_inductance_within_1_percent_of_zoh_at_alternative_frequency)
{
    const float rTerminal = 0.073f * 1.5f;
    const float lTerminalMH = 0.5f * 1.5f;

    services::SinusoidalInductanceEstimator::Config config{
        hal::Hertz{ 500 }, hal::Percent{ 15 }, 5, 20, 1, services::WindingConfiguration::Wye
    };

    auto [result, expectedL] = RunPlantSimulation(driverMock, estimator, config, rTerminal, lTerminalMH);

    ASSERT_TRUE(result.inductance.has_value());
    EXPECT_NEAR(result.inductance->Value(), expectedL, expectedL * 0.01f);
}

TEST_F(SinusoidalInductanceEstimatorTest, fitQuality_drops_for_unexpected_signal)
{
    // If the current is a DC offset instead of a sinusoid at f_inj, coherence should be low.
    services::SinusoidalInductanceEstimator::Config config{
        hal::Hertz{ 700 }, hal::Percent{ 15 }, 2, 5, 1, services::WindingConfiguration::Wye
    };

    const float fInj = static_cast<float>(config.injectionFrequency.Value());
    const auto samplesPerPeriod = static_cast<std::size_t>(std::round(fs / fInj));
    const auto totalSamples = (config.warmupPeriods + config.measurementPeriods) * samplesPerPeriod;

    services::SinusoidalInductanceEstimator::Result result{};

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& cb) { driverMock.StorePhaseCurrentsCallback(cb); });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(totalSamples);
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [&result](auto r) { result = r; });

    // Provide a DC current signal (incoherent with f_inj).
    for (std::size_t k = 0; k < totalSamples; ++k)
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_LT(result.fitQuality, 0.5f);
}
