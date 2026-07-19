#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr std::size_t stepsPerRevolution = 12;
    constexpr auto anglePerStep = twoPi / static_cast<float>(stepsPerRevolution);
    constexpr float minRotationThreshold = std::numbers::pi_v<float> / 2.0f;

    // Center-aligned half-bridge: applied alpha voltage amplitude = modIndex * Vdc / 2.
    constexpr float voltsPerModulation = 0.5f;

    // Bound modulation so every leg duty stays within [20, 80] % (keeps the low-side shunt samplable).
    constexpr float maxSafeModIndex = 0.6f;

    foc::PhasePwmDutyCycles NormalizedDutyCycles(foc::ThreePhase voltages)
    {
        auto offset = 50.0f;
        auto dutyA = static_cast<uint8_t>(std::clamp(offset + voltages.a * 50.0f, 0.0f, 100.0f));
        auto dutyB = static_cast<uint8_t>(std::clamp(offset + voltages.b * 50.0f, 0.0f, 100.0f));
        auto dutyC = static_cast<uint8_t>(std::clamp(offset + voltages.c * 50.0f, 0.0f, 100.0f));
        return foc::PhasePwmDutyCycles{ hal::Percent{ dutyA }, hal::Percent{ dutyB }, hal::Percent{ dutyC } };
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

        const auto injectionHz = rlConfig.injectionFrequency.Value();
        if (injectionHz == 0 || samplingFrequencyHz % injectionHz != 0)
        {
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        const auto samplesPerPeriod = samplingFrequencyHz / injectionHz;

        injectionModIndex = std::min(static_cast<float>(rlConfig.injectionVoltagePercent.Value()) / 100.0f, maxSafeModIndex);
        angularFrequency = twoPi * static_cast<float>(injectionHz);
        phaseIncrement = angularFrequency / static_cast<float>(samplingFrequencyHz);
        warmupSamples = rlConfig.warmupPeriods * samplesPerPeriod;
        measurementSamples = rlConfig.measurementPeriods * samplesPerPeriod;

        const auto maxCurrent = driver.MaxCurrentSupported().Value();
        maxCurrentSquared = maxCurrent * maxCurrent;

        phase = 0.0f;
        // Demod reference lags the applied phase by the PWM->ADC pipeline delay, cancelling the
        // ~2*pi*f_inj/f_s phase error that would otherwise bias R.
        demodPhase = std::fmod(-static_cast<float>(rlConfig.voltageToCurrentDelaySamples) * phaseIncrement, twoPi);
        if (demodPhase < 0.0f)
            demodPhase += twoPi;
        sampleIndex = 0;
        sumSin = 0.0f;
        sumCos = 0.0f;
        sumSq = 0.0f;

        driver.Stop();
        driver.PhaseCurrentsReady(hal::Hertz{ static_cast<uint32_t>(samplingFrequencyHz) }, [this](auto currentPhases)
            {
                OnHfSample(currentPhases);
            });
        // Contract: the PWM must be driven once after registering the callback, otherwise no phase
        // currents are ever produced and the callback never fires.
        ApplyInjectionVoltage();
    }

    OPTIMIZE_FOR_SPEED void ElectricalParametersIdentificationImpl::ApplyInjectionVoltage()
    {
        driver.ThreePhasePwmOutput(NormalizedDutyCycles(clarke.Inverse(foc::TwoPhase{ injectionModIndex * foc::FastTrigonometry::Sine(phase), 0.0f })));
    }

    OPTIMIZE_FOR_SPEED void ElectricalParametersIdentificationImpl::OnHfSample(const foc::PhaseCurrents& currentPhases)
    {
        if (sampleIndex >= warmupSamples + measurementSamples)
            return;

        const float a = currentPhases.a.Value();
        const float b = currentPhases.b.Value();
        const float c = currentPhases.c.Value();

        const float peakSquared = std::max({ a * a, b * b, c * c });
        if (peakSquared > maxCurrentSquared)
        {
            AbortResistanceAndInductance();
            return;
        }

        ApplyInjectionVoltage();

        if (sampleIndex >= warmupSamples)
        {
            const float iAlpha = clarke.Forward(foc::ThreePhase{ a, b, c }).alpha;
            sumSin += iAlpha * foc::FastTrigonometry::Sine(demodPhase);
            sumCos += iAlpha * foc::FastTrigonometry::Cosine(demodPhase);
            sumSq += iAlpha * iAlpha;
        }

        phase += phaseIncrement;
        if (phase >= twoPi)
            phase -= twoPi;

        demodPhase += phaseIncrement;
        if (demodPhase >= twoPi)
            demodPhase -= twoPi;

        ++sampleIndex;
        if (sampleIndex >= warmupSamples + measurementSamples)
        {
            driver.Stop();
            ComputeAndReport();
        }
    }

    void ElectricalParametersIdentificationImpl::AbortResistanceAndInductance()
    {
        sampleIndex = warmupSamples + measurementSamples;
        driver.Stop();
        if (onResistanceAndInductanceDone)
            onResistanceAndInductanceDone(std::nullopt);
    }

    void ElectricalParametersIdentificationImpl::ComputeAndReport()
    {
        if (!onResistanceAndInductanceDone)
            return;

        const auto n = static_cast<float>(measurementSamples);
        const float iRe = 2.0f * sumSin / n;
        const float iIm = 2.0f * sumCos / n;
        const float magnitudeSquared = iRe * iRe + iIm * iIm;

        if (magnitudeSquared < minDemodulatedCurrent * minDemodulatedCurrent)
        {
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        const float amplitude = injectionModIndex * voltsPerModulation * vdc.Value();
        float resistance = amplitude * iRe / magnitudeSquared;
        float inductance = -amplitude * iIm / (angularFrequency * magnitudeSquared);

        // THD-like residual (0 = perfect sinusoid), diagnostic only: demod already rejects the
        // low-frequency back-EMF, so a raised residual flags a disturbance without invalidating R/Ls.
        const float fundamentalEnergy = n * magnitudeSquared / 2.0f;
        const float fitQuality = std::abs(sumSq - fundamentalEnergy) / fundamentalEnergy;

        const float correction = (rlConfig.windingConfig == WindingConfiguration::Delta) ? deltaCoefficient : 1.0f;
        resistance *= correction;
        inductance *= correction;

        if (resistance <= 0.0f || inductance <= 0.0f)
        {
            onResistanceAndInductanceDone(std::nullopt);
            return;
        }

        onResistanceAndInductanceDone(ResistanceInductanceResult{
            foc::Ohm{ resistance },
            foc::MilliHenry{ inductance * 1000.0f },
            foc::Volts{ 0.0f },
            fitQuality });
    }

    void ElectricalParametersIdentificationImpl::EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone)
    {
        polePairsConfig = config;
        onPolePairsDone = onDone;
        currentSampleIndex = 0;
        accumulatedRotation = 0.0f;

        previousPosition = encoder.Read();

        driver.Stop();
        driver.PhaseCurrentsReady(hal::Hertz{ static_cast<uint32_t>(samplingFrequencyHz) }, [](auto) {});
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
