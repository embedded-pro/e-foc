#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <cmath>
#include <numbers>
#include <numeric>

namespace
{
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr std::size_t stepsPerRevolution = 12;
    constexpr auto anglePerStep = twoPi / static_cast<float>(stepsPerRevolution);
    constexpr float minRotationThreshold = std::numbers::pi_v<float> / 2.0f;
    constexpr float timeConstantThreshold = 0.632f;
    const hal::Hertz samplingFrequency{ 10000 };
    const auto samplingPeriod = 1.0f / static_cast<float>(samplingFrequency.Value());

    foc::PhasePwmDutyCycles
    NormalizedDutyCycles(foc::ThreePhase voltages)
    {
        auto offset = 50.0f;
        auto dutyA = static_cast<uint8_t>(std::clamp(offset + voltages.a * 50.0f, 0.0f, 100.0f));
        auto dutyB = static_cast<uint8_t>(std::clamp(offset + voltages.b * 50.0f, 0.0f, 100.0f));
        auto dutyC = static_cast<uint8_t>(std::clamp(offset + voltages.c * 50.0f, 0.0f, 100.0f));
        return foc::PhasePwmDutyCycles{ hal::Percent{ dutyA }, hal::Percent{ dutyB }, hal::Percent{ dutyC } };
    }

    float AverageAndRemoveFront(infra::BoundedDeque<float>& deque)
    {
        float sum = 0.0f;

        for (const auto& samples : deque)
            sum += samples;

        float average = sum / static_cast<float>(deque.size());

        deque.pop_front();

        return average;
    }

    float GetSteadyStateCurrent(const infra::BoundedVector<float>& samples)
    {
        auto lastQuarter = static_cast<std::size_t>(static_cast<float>(samples.size()) * 0.9f);

        return std::accumulate(samples.begin() + lastQuarter, samples.end(), 0.0f) / static_cast<float>(samples.size() - lastQuarter);
    }

    std::optional<float> GetTauFromCurrentSamples(const infra::BoundedVector<float>& samples, float steadyStateCurrent, std::size_t averageFilter)
    {
        auto targetCurrent = timeConstantThreshold * steadyStateCurrent;

        // The buffer is averaged only once full and then pops its front, so filtered index n spans raw
        // samples n..n+N-1 and represents the raw instant n + (N-1)/2.
        const auto groupDelay = static_cast<float>(averageFilter - 1) / 2.0f;

        for (std::size_t i = 0; i < samples.size(); ++i)
            if (samples[i] >= targetCurrent)
                return static_cast<float>(i) + groupDelay;

        return std::nullopt;
    }

    std::optional<foc::Ohm> CalculateResistance(float voltage, float current)
    {
        return foc::Ohm{ voltage / current };
    }

    std::optional<foc::MilliHenry> CalculateInductance(foc::Ohm resistance, float tau)
    {
        return foc::MilliHenry{ resistance.Value() * tau * samplingPeriod * 1000.0f };
    }
}

namespace services
{
    ElectricalParametersIdentificationImpl::ElectricalParametersIdentificationImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder, foc::Volts vdc)
        : driver(driver)
        , encoder(encoder)
        , vdc(vdc)
    {
    }

    void ElectricalParametersIdentificationImpl::EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)>& onDone)
    {
        if (rlRunning)
        {
            onDone(std::nullopt, std::nullopt);
            return;
        }
        rlRunning = true;
        resistanceAndInductanceConfig = config;
        onResistanceAndInductanceDone = onDone;
        currentSamples.clear();
        filteredCurrentSample.clear();

        driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
        driver.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{
            hal::Percent{ neutralDuty },
            hal::Percent{ neutralDuty },
            hal::Percent{ neutralDuty } });

        settleTimer.Start(resistanceAndInductanceConfig.settleTime, [this]()
            {
                driver.PhaseCurrentsReady(samplingFrequency, [this](auto currentPhases)
                    {
                        currentSamples.push_back(currentPhases.a.Value());

                        if (currentSamples.full())
                            filteredCurrentSample.push_back(AverageAndRemoveFront(currentSamples));

                        if (filteredCurrentSample.full())
                            AnalyzeInductanceMeasures();
                    });

                driver.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{
                    hal::Percent{ resistanceAndInductanceConfig.testVoltagePercent.Value() },
                    hal::Percent{ neutralDuty },
                    hal::Percent{ neutralDuty } });
            });
    }

    void ElectricalParametersIdentificationImpl::AnalyzeInductanceMeasures()
    {
        driver.Stop();

        auto steadyStateCurrent = GetSteadyStateCurrent(filteredCurrentSample);

        if (steadyStateCurrent <= 0.0f)
        {
            rlRunning = false;
            onResistanceAndInductanceDone(std::nullopt, std::nullopt);
        }
        else
        {
            auto tau = GetTauFromCurrentSamples(filteredCurrentSample, steadyStateCurrent, averageFilter);
            const auto appliedDuty = static_cast<float>(resistanceAndInductanceConfig.testVoltagePercent.Value() - neutralDuty);
            const auto terminalFactor = resistanceAndInductanceConfig.windingConfig == WindingConfiguration::Delta ? deltaTerminalFactor : wyeTerminalFactor;
            auto terminalResistance = CalculateResistance(appliedDuty * vdc.Value() / 100.0f, steadyStateCurrent);
            auto resistance = terminalResistance.has_value() ? std::optional<foc::Ohm>{ foc::Ohm{ terminalResistance->Value() / terminalFactor } } : std::nullopt;

            rlRunning = false;
            if (resistance.has_value())
                onResistanceAndInductanceDone(resistance,
                    tau.has_value() ? CalculateInductance(resistance.value(), tau.value()) : std::optional<foc::MilliHenry>{});
            else
                onResistanceAndInductanceDone(std::nullopt, std::nullopt);

            filteredCurrentSample.clear();
        }
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
        const std::size_t totalSteps = polePairsConfig.electricalRevolutions * stepsPerRevolution;

        if (currentSampleIndex < totalSteps)
            RunPolePairLogic();
        else
            CalculatePolePairs();
    }

    void ElectricalParametersIdentificationImpl::RunPolePairLogic()
    {
        auto electricalAngle = static_cast<float>(currentSampleIndex) * anglePerStep;
        auto voltage = static_cast<float>(polePairsConfig.testVoltagePercent.Value()) / 100.0f;

        driver.ThreePhasePwmOutput(NormalizedDutyCycles(transforms.Inverse(foc::RotatingFrame{ voltage, 0.0f }, std::cos(electricalAngle), std::sin(electricalAngle))));

        settleTimer.Start(polePairsConfig.settleTimeBetweenSteps, [this]()
            {
                auto currentPosition = encoder.Read();
                auto delta = currentPosition.Value() - previousPosition.Value();

                delta = delta - twoPi * std::floor((delta + std::numbers::pi_v<float>) / twoPi);

                accumulatedRotation += delta;
                previousPosition = currentPosition;

                currentSampleIndex++;
                ApplyNextElectricalAngle();
            });
    }

    void ElectricalParametersIdentificationImpl::CalculatePolePairs()
    {
        driver.Stop();

        if (onPolePairsDone)
        {
            auto mechanicalRotation = std::abs(accumulatedRotation);
            polePairsRunning = false;

            if (mechanicalRotation > minRotationThreshold)
            {
                auto electricalRevolutions = static_cast<float>(polePairsConfig.electricalRevolutions);
                auto mechanicalRevolutions = mechanicalRotation / twoPi;

                auto polePairs = static_cast<std::size_t>(std::round(electricalRevolutions / mechanicalRevolutions));
                onPolePairsDone(std::make_optional<std::size_t>(polePairs));
            }
            else
                onPolePairsDone(std::nullopt);
        }
    }
}
