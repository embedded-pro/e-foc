#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/math/Matrix.hpp"
#include <cmath>
#include <numbers>

namespace
{
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr std::size_t stepsPerRevolution = 12;
    constexpr auto anglePerStep = twoPi / static_cast<float>(stepsPerRevolution);
    constexpr float minRotationThreshold = std::numbers::pi_v<float> / 2.0f;
    constexpr float minSteadyStateCurrent = 0.001f;
    constexpr float safeMinDutyPercent = 5.0f;
    constexpr float safeMaxDutyPercent = 80.0f;
    const hal::Hertz samplingFrequency{ 10000 };
    const auto samplingPeriod = 1.0f / static_cast<float>(samplingFrequency.Value());

    foc::PhasePwmDutyCycles NormalizedDutyCycles(foc::ThreePhase voltages)
    {
        auto offset = 50.0f;
        auto dutyA = static_cast<uint8_t>(std::clamp(offset + voltages.a * 50.0f, 0.0f, 100.0f));
        auto dutyB = static_cast<uint8_t>(std::clamp(offset + voltages.b * 50.0f, 0.0f, 100.0f));
        auto dutyC = static_cast<uint8_t>(std::clamp(offset + voltages.c * 50.0f, 0.0f, 100.0f));
        return foc::PhasePwmDutyCycles{ hal::Percent{ dutyA }, hal::Percent{ dutyB }, hal::Percent{ dutyC } };
    }

    float MeanMagnitude(const infra::BoundedVector<float>& samples)
    {
        float sum = 0.0f;
        for (const auto& v : samples)
            sum += std::abs(v);
        return sum / static_cast<float>(samples.size());
    }

    float SteadyStateMagnitude(const infra::BoundedVector<float>& transient)
    {
        const auto start = static_cast<std::size_t>(static_cast<float>(transient.size()) * 0.9f);
        float sum = 0.0f;
        for (std::size_t i = start; i < transient.size(); ++i)
            sum += transient[i];
        return sum / static_cast<float>(transient.size() - start);
    }

    float IntegralInductance(const infra::BoundedVector<float>& transient, float steadyState, float resistance)
    {
        float integral = 0.0f;
        for (const auto& v : transient)
            integral += (steadyState - v) * samplingPeriod;
        return resistance * integral / steadyState;
    }
}

namespace services
{
    ElectricalParametersIdentificationImpl::ElectricalParametersIdentificationImpl(foc::ThreePhaseInverter& driver, foc::Encoder& encoder, foc::Volts vdc)
        : driver(driver)
        , encoder(encoder)
        , vdc(vdc)
    {
    }

    void ElectricalParametersIdentificationImpl::EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<ResistanceInductanceResult>)>& onDone)
    {
        rlConfig = config;
        onResistanceAndInductanceDone = onDone;
        probeBuffer.clear();

        StartProbeStep();
    }

    void ElectricalParametersIdentificationImpl::StartProbeStep()
    {
        driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
        driver.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{
            hal::Percent{ rlConfig.probeVoltagePercent.Value() },
            hal::Percent{ neutralDuty },
            hal::Percent{ neutralDuty } });

        driver.PhaseCurrentsReady(samplingFrequency, [this](auto currentPhases)
            {
                probeBuffer.push_back(std::abs(currentPhases.a.Value()));
                if (probeBuffer.full())
                    OnProbeBufferFull();
            });
    }

    void ElectricalParametersIdentificationImpl::OnProbeBufferFull()
    {
        const float probeCurrent = SteadyStateMagnitude(probeBuffer);
        if (probeCurrent < minSteadyStateCurrent)
        {
            driver.Stop();
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        const float probeVoltage = static_cast<float>(rlConfig.probeVoltagePercent.Value()) / 100.0f * vdc.Value();
        rCoarse = probeVoltage / probeCurrent;

        StartLevel(0);
    }

    void ElectricalParametersIdentificationImpl::StartLevel(std::size_t level)
    {
        levelBatch.clear();

        const float targetCurrent = rlConfig.targetCurrentFractions[level] * driver.MaxCurrentSupported().Value();
        const float rawDuty = (targetCurrent * rCoarse / vdc.Value()) * 100.0f + static_cast<float>(neutralDuty);
        const auto duty = static_cast<uint8_t>(std::clamp(rawDuty, safeMinDutyPercent, safeMaxDutyPercent));

        levelVoltages[level] = (static_cast<float>(duty) - static_cast<float>(neutralDuty)) / 100.0f * vdc.Value();

        driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
        driver.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{
            hal::Percent{ duty },
            hal::Percent{ neutralDuty },
            hal::Percent{ neutralDuty } });

        settleTimer.Start(rlConfig.settlePerLevel, [this, level]()
            {
                driver.PhaseCurrentsReady(samplingFrequency, [this, level](auto currentPhases)
                    {
                        levelBatch.push_back(std::abs(currentPhases.a.Value()));
                        if (levelBatch.full())
                            OnLevelBatchFull(level);
                    });
            });
    }

    void ElectricalParametersIdentificationImpl::OnLevelBatchFull(std::size_t level)
    {
        levelSteadyStateCurrents[level] = MeanMagnitude(levelBatch);

        if (level + 1 < numLevels)
            StartLevel(level + 1);
        else
        {
            driver.Stop();
            ComputeAndReport();
        }
    }

    bool ElectricalParametersIdentificationImpl::FitResistance()
    {
        math::Matrix<float, numLevels, 1> currents;
        math::Matrix<float, numLevels, 1> voltages;
        for (std::size_t j = 0; j < numLevels; ++j)
        {
            if (levelSteadyStateCurrents[j] < minSteadyStateCurrent)
                return false;
            currents.at(j, 0) = levelSteadyStateCurrents[j];
            voltages.at(j, 0) = levelVoltages[j];
        }

        estimators::LinearRegression<float, numLevels, 1> regression;
        regression.Fit(currents, voltages);

        fittedVoltageOffset = regression.Coefficients().at(0, 0);
        fittedResistance = regression.Coefficients().at(1, 0);

        return fittedResistance > 0.0f;
    }

    float ElectricalParametersIdentificationImpl::ResistanceFitResidual() const
    {
        float maxResidual = 0.0f;
        for (std::size_t j = 0; j < numLevels; ++j)
        {
            const float predicted = fittedResistance * levelSteadyStateCurrents[j] + fittedVoltageOffset;
            maxResidual = std::max(maxResidual, std::abs(levelVoltages[j] - predicted));
        }
        return maxResidual / fittedResistance;
    }

    void ElectricalParametersIdentificationImpl::ComputeAndReport()
    {
        if (!FitResistance())
        {
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        const float fitQuality = ResistanceFitResidual();
        if (fitQuality > maxAcceptableFitResidual)
        {
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        const float steadyState = SteadyStateMagnitude(probeBuffer);
        const float inductance = IntegralInductance(probeBuffer, steadyState, fittedResistance);

        const float correction = (rlConfig.windingConfig == WindingConfiguration::Delta) ? deltaCoefficient : 1.0f;
        const float resistancePhase = fittedResistance * correction;
        const float inductancePhase = inductance * correction;

        if (inductancePhase <= 0.0f)
        {
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        onResistanceAndInductanceDone(ResistanceInductanceResult{
            foc::Ohm{ resistancePhase },
            foc::MilliHenry{ inductancePhase * 1000.0f },
            foc::Volts{ fittedVoltageOffset },
            fitQuality });
    }

    void ElectricalParametersIdentificationImpl::EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone)
    {
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
