#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/services/electrical_system_ident/SinusoidalInductanceEstimator.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    const hal::Hertz samplingFrequency{ 10000 };
}

namespace services
{
    SinusoidalInductanceEstimator::SinusoidalInductanceEstimator(drivers::ThreePhaseInverter& driver, foc::Volts vdc)
        : driver(driver)
        , vdc(vdc)
    {}

    void SinusoidalInductanceEstimator::Start(const Config& config, const infra::Function<void(Result)>& onDone)
    {
        activeConfig = config;
        this->onDone = onDone;

        const auto fs = static_cast<float>(samplingFrequency.Value());
        const auto fInj = static_cast<float>(config.injectionFrequency.Value());

        // Snap to an integer samples-per-period so the Goertzel bin and the injection are
        // at the same frequency; any mismatch causes spectral leakage that biases Im(Z).
        const auto samplesPerPeriod = fInj > 0.0f
                                          ? static_cast<std::size_t>(std::round(fs / fInj))
                                          : std::size_t{ 0 };

        if (samplesPerPeriod == 0)
        {
            onDone(Result{});
            return;
        }
        omega = twoPi * fs / static_cast<float>(samplesPerPeriod);
        phaseIncrement = omega / fs;
        injectionPhase = 0.0f;

        // Clarke inverse of {v, 0} gives ThreePhase{v, -v/2, -v/2}; after NormalizedDutyCycles
        // the terminal A-to-BC amplitude is v * 0.75 * Vdc.
        injectionAmplitude = static_cast<float>(config.injectionVoltagePercent.Value()) / 100.0f;
        vTerminalAmplitude = injectionAmplitude * 0.75f * vdc.Value();

        terminalFactor = config.windingConfig == WindingConfiguration::Delta
                             ? deltaTerminalFactor
                             : wyeTerminalFactor;

        warmupSamples = config.warmupPeriods * samplesPerPeriod;
        measurementSamples = config.measurementPeriods * samplesPerPeriod;

        sampleCount = 0;
        sumSquared = 0.0f;

        goertzel.emplace(config.measurementPeriods, measurementSamples);

        driver.PhaseCurrentsReady(samplingFrequency, [this](auto currents)
            {
                OnCurrentSample(currents);
            });
    }

    void SinusoidalInductanceEstimator::Abort()
    {
        if (!onDone)
            return;

        driver.Stop();
        onDone = nullptr;
    }

    void SinusoidalInductanceEstimator::OnCurrentSample(foc::PhaseCurrents currents)
    {
        if (!onDone)
            return;

        const float vNorm = injectionAmplitude * math::Sin(injectionPhase);
        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ vNorm, 0.0f }, 1.0f, 0.0f)));

        injectionPhase += phaseIncrement;
        if (injectionPhase >= twoPi)
            injectionPhase -= twoPi;

        ++sampleCount;

        if (sampleCount <= warmupSamples)
            return;

        const float iAlpha = currents.a.Value();
        goertzel->Push(iAlpha);
        sumSquared += iAlpha * iAlpha;

        if (goertzel->Ready())
        {
            driver.Stop();
            onDone(ComputeResult());
        }
    }

    SinusoidalInductanceEstimator::Result SinusoidalInductanceEstimator::ComputeResult() const
    {
        auto I = goertzel->Result();

        // Rotate by +ω·d·Ts to cancel the d-sample ADC pipeline lag.
        const float delayAngle = omega * static_cast<float>(activeConfig.voltageToCurrentDelaySamples) / static_cast<float>(samplingFrequency.Value());
        const float cosD = math::Cos(delayAngle);
        const float sinD = math::Sin(delayAngle);
        const float iRe = I.Real() * cosD - I.Imaginary() * sinD;
        const float iIm = I.Real() * sinD + I.Imaginary() * cosD;

        const float magSquared = iRe * iRe + iIm * iIm;
        if (magSquared < 1e-20f)
            return Result{};

        const auto N = static_cast<float>(measurementSamples);
        const float zImag = -vTerminalAmplitude * N / 2.0f * iRe / magSquared;

        const float fitQuality = (sumSquared > 0.0f)
                                     ? std::clamp(2.0f * magSquared / (N * sumSquared), 0.0f, 1.0f)
                                     : 0.0f;

        if (zImag <= 0.0f)
            return Result{ std::nullopt, fitQuality };

        const float lPhase = zImag / (omega * terminalFactor);
        return Result{ foc::MilliHenry{ lPhase * 1000.0f }, fitQuality };
    }
}
