#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/services/electrical_system_ident/SinusoidalInductanceEstimator.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
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

        if (!InitializeParameters())
        {
            onDone(Result{});
            return;
        }

        BeginInjection();
    }

    bool SinusoidalInductanceEstimator::InitializeParameters()
    {
        const auto fs = static_cast<float>(samplingFrequency.Value());
        const auto fInj = static_cast<float>(activeConfig.injectionFrequency.Value());
        const auto samplesPerPeriod = fInj > 0.0f
                                          ? static_cast<std::size_t>(std::round(fs / fInj))
                                          : std::size_t{ 0 };

        if (samplesPerPeriod == 0)
            return false;

        omega = twoPi * fs / static_cast<float>(samplesPerPeriod);
        phaseIncrement = omega / fs;
        injectionPhase = 0.0f;
        injectionAmplitude = static_cast<float>(activeConfig.injectionVoltagePercent.Value()) / 100.0f;
        vTerminalAmplitude = injectionAmplitude * 0.75f * vdc.Value();
        terminalFactor = activeConfig.windingConfig == WindingConfiguration::Delta
                             ? deltaTerminalFactor
                             : wyeTerminalFactor;
        warmupSamples = activeConfig.warmupPeriods * samplesPerPeriod;
        measurementSamples = activeConfig.measurementPeriods * samplesPerPeriod;
        sampleCount = 0;
        sumSquared = 0.0f;
        goertzel.emplace(activeConfig.measurementPeriods, measurementSamples);

        return true;
    }

    void SinusoidalInductanceEstimator::BeginInjection()
    {
        driver.PhaseCurrentsReady(samplingFrequency, [this](auto currents)
            {
                OnCurrentSample(currents);
            });

        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ 0.0f, 0.0f }, 1.0f, 0.0f)));

        noSampleTimer.Start(activeConfig.noSampleTimeout, [this]()
            {
                FailMeasurement();
            });
    }

    void SinusoidalInductanceEstimator::Abort()
    {
        if (!onDone)
            return;

        noSampleTimer.Cancel();
        driver.Stop();
        onDone = nullptr;
    }

    void SinusoidalInductanceEstimator::FailMeasurement()
    {
        driver.Stop();
        if (onDone)
            onDone(Result{});
    }

    void SinusoidalInductanceEstimator::AdvanceInjection()
    {
        noSampleTimer.Start(activeConfig.noSampleTimeout, [this]()
            {
                FailMeasurement();
            });

        const float vNorm = injectionAmplitude * math::Sin(injectionPhase);
        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ vNorm, 0.0f }, 1.0f, 0.0f)));

        injectionPhase += phaseIncrement;
        if (injectionPhase >= twoPi)
            injectionPhase -= twoPi;

        ++sampleCount;
    }

    void SinusoidalInductanceEstimator::OnCurrentSample(foc::PhaseCurrents currents)
    {
        if (!onDone)
            return;

        if (ExceedsInjectionLimit(currents, driver.MaxCurrentSupported()))
        {
            noSampleTimer.Cancel();
            driver.Stop();
            onDone(Result{});
            return;
        }

        AdvanceInjection();

        if (sampleCount <= warmupSamples)
            return;

        const float iAlpha = currents.a.Value();
        goertzel->Push(iAlpha);
        sumSquared += iAlpha * iAlpha;

        if (goertzel->Ready())
        {
            noSampleTimer.Cancel();
            driver.Stop();
            onDone(ComputeResult());
        }
    }

    SinusoidalInductanceEstimator::Result SinusoidalInductanceEstimator::ComputeResult() const
    {
        auto I = goertzel->Result();

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
