#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <cmath>
#include <numbers>

namespace
{
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr std::size_t stepsPerRevolution = 12;
    constexpr float anglePerStep = twoPi / static_cast<float>(stepsPerRevolution);
    constexpr float minRotationThreshold = std::numbers::pi_v<float> / 2.0f;
    const hal::Hertz samplingFrequency{ 10000 };
}

namespace services
{
    ElectricalParametersIdentificationImpl::ElectricalParametersIdentificationImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder, foc::Volts vdc)
        : driver(driver)
        , encoder(encoder)
        , vdc(vdc)
        , resistanceEstimator(driver, vdc)
        , inductanceEstimator(driver, vdc)
    {}

    void ElectricalParametersIdentificationImpl::EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(ResistanceInductanceResult)>& onDone)
    {
        if (rlRunning)
        {
            onDone(ResistanceInductanceResult{});
            return;
        }
        rlRunning = true;
        rlConfig = config;
        onResistanceAndInductanceDone = onDone;
        pendingResult = ResistanceInductanceResult{};

        resistanceEstimator.Start(
            ResistanceEstimator::Config{ config.testVoltagePercent, config.settleTime, config.windingConfig },
            [this](auto result)
            {
                OnResistanceDone(result);
            });
    }

    void ElectricalParametersIdentificationImpl::OnResistanceDone(ResistanceEstimator::Result result)
    {
        pendingResult.resistance = result.resistance;

        if (!result.resistance.has_value())
        {
            rlRunning = false;
            onResistanceAndInductanceDone(pendingResult);
            return;
        }

        inductanceEstimator.Start(
            SinusoidalInductanceEstimator::Config{
                rlConfig.injectionFrequency,
                rlConfig.injectionVoltagePercent,
                rlConfig.warmupPeriods,
                rlConfig.measurementPeriods,
                rlConfig.voltageToCurrentDelaySamples,
                rlConfig.windingConfig },
            [this](SinusoidalInductanceEstimator::Result lResult)
            {
                pendingResult.inductance = lResult.inductance;
                pendingResult.fitQuality = lResult.fitQuality;
                rlRunning = false;
                onResistanceAndInductanceDone(pendingResult);
            });
    }

    void ElectricalParametersIdentificationImpl::EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone)
    {
        if (polePairsRunning)
        {
            onDone(std::nullopt);
            return;
        }
        polePairsRunning = true;
        polePairsConfig = config;
        onPolePairsDone = onDone;
        currentSampleIndex = 0;
        accumulatedRotation = 0.0f;

        initialPosition = encoder.Read();
        previousPosition = initialPosition;

        driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
        ApplyNextElectricalAngle();
    }

    void ElectricalParametersIdentificationImpl::ApplyNextElectricalAngle()
    {
        const auto totalSteps = polePairsConfig.electricalRevolutions * stepsPerRevolution;

        if (currentSampleIndex < totalSteps)
            RunPolePairLogic();
        else
            CalculatePolePairs();
    }

    void ElectricalParametersIdentificationImpl::RunPolePairLogic()
    {
        const float electricalAngle = static_cast<float>(currentSampleIndex) * anglePerStep;
        const float voltage = static_cast<float>(polePairsConfig.testVoltagePercent.Value()) / 100.0f;

        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ voltage, 0.0f }, std::cos(electricalAngle), std::sin(electricalAngle))));

        settleTimer.Start(polePairsConfig.settleTimeBetweenSteps, [this]()
            {
                const auto currentPosition = encoder.Read();
                auto delta = currentPosition.Value() - previousPosition.Value();
                delta = delta - twoPi * std::floor((delta + std::numbers::pi_v<float>) / twoPi);

                accumulatedRotation += delta;
                previousPosition = currentPosition;
                ++currentSampleIndex;
                ApplyNextElectricalAngle();
            });
    }

    void ElectricalParametersIdentificationImpl::CalculatePolePairs()
    {
        driver.Stop();

        if (!onPolePairsDone)
            return;

        const float mechanicalRotation = std::abs(accumulatedRotation);
        polePairsRunning = false;

        if (mechanicalRotation > minRotationThreshold)
        {
            const auto electricalRevolutions = static_cast<float>(polePairsConfig.electricalRevolutions);
            const float mechanicalRevolutions = mechanicalRotation / twoPi;
            onPolePairsDone(std::make_optional<std::size_t>(
                static_cast<std::size_t>(std::round(electricalRevolutions / mechanicalRevolutions))));
        }
        else
            onPolePairsDone(std::nullopt);
    }
}
