#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <cmath>

namespace services
{
    MotorAlignmentImpl::MotorAlignmentImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder)
        : driver(driver)
        , encoder(encoder)
    {
    }

    void MotorAlignmentImpl::ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone)
    {
        if (onAlignmentDone)
        {
            onDone(std::nullopt);
            return;
        }

        this->polePairs = polePairs;
        alignmentConfig = config;
        ++run;
        finishing = false;
        currentSampleIndex = 0;
        consecutiveSettledSamples = 0;
        settledSumA = settledSumB = settledSumC = 0.0f;
        phaseMissing = false;
        previousPosition = encoder.Read();
        onAlignmentDone = onDone;

        timeoutTimer.Start(config.timeout, [this]()
            {
                FailToConverge();
            });

        ApplyAlignmentVoltage();
    }

    void MotorAlignmentImpl::ApplyAlignmentVoltage()
    {
        auto voltage = foc::DutyFraction(alignmentConfig.testVoltage);
        auto electricalAngle = alignmentAngle;

        driver.Stop();
        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ voltage, 0.0f }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle))));

        driver.PhaseCurrentsReady(alignmentConfig.samplingFrequency, [this](auto currents)
            {
                if (!onAlignmentDone || finishing)
                    return;

                if (ExceedsInjectionLimit(currents, driver.MaxCurrentSupported()))
                {
                    Finish(std::nullopt);
                    return;
                }

                currentSampleIndex++;

                if (currentSampleIndex >= alignmentConfig.maxSamples)
                    Finish(std::nullopt);
                else
                    ProcessPosition(currents);
            });
    }

    void MotorAlignmentImpl::ProcessPosition(const foc::PhaseCurrents& currents)
    {
        auto currentPosition = encoder.Read();
        auto positionChange = std::abs(foc::detail::PositionWithWrapAround((currentPosition - previousPosition).Value()));
        previousPosition = currentPosition;

        if (positionChange < alignmentConfig.settledThreshold.Value())
        {
            consecutiveSettledSamples++;
            settledSumA += currents.a.Value();
            settledSumB += currents.b.Value();
            settledSumC += currents.c.Value();

            if (consecutiveSettledSamples >= alignmentConfig.settledCount)
                Settle();
        }
        else
        {
            consecutiveSettledSamples = 0;
            settledSumA = settledSumB = settledSumC = 0.0f;
        }
    }

    // Interrupt context: the vector keeps holding the rotor while the event loop judges the snapshot.
    void MotorAlignmentImpl::Settle()
    {
        finishing = true;
        settledEndSample = currentSampleIndex;

        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [settledRun = run](const infra::SharedPtr<MotorAlignmentImpl>& self)
            {
                if (self->run == settledRun && self->onAlignmentDone)
                    self->Decide();
            },
            WeakFromThis());
    }

    void MotorAlignmentImpl::Decide()
    {
        if (!CarriesNoCurrent())
        {
            CalculateAlignmentOffset();
            return;
        }

        phaseMissing = true;
        driver.Stop();
        Complete(std::nullopt);
    }

    bool MotorAlignmentImpl::CarriesNoCurrent() const
    {
        const auto samples = static_cast<float>(consecutiveSettledSamples);
        const auto a = std::abs(settledSumA) / samples;
        const auto b = std::abs(settledSumB) / samples;
        const auto c = std::abs(settledSumC) / samples;
        const auto final = ExpectedPhaseACurrent();
        const auto expected = final.has_value() ? std::make_optional(*final * SettledShareOfFinalCurrent()) : std::nullopt;

        if (expected.has_value() && a < missingFraction * *expected)
            return true;

        // b and c each return half of phase a's current; without a model, a measured a is the only scale
        const auto floor = missingFraction * 0.5f * expected.value_or(a);
        return b < floor || c < floor;
    }

    std::optional<float> MotorAlignmentImpl::ExpectedPhaseACurrent() const
    {
        const auto resistance = alignmentConfig.phaseResistance.Value();
        const auto bus = alignmentConfig.busVoltage.Value();

        if (resistance <= 0.0f || bus <= 0.0f)
            return std::nullopt;

        // The vector puts d·Vdc/2 across phase a and nothing turns, so only the winding resistance limits the current
        return 0.5f * foc::DutyFraction(alignmentConfig.testVoltage) * bus / resistance;
    }

    // The winding current rises with L/R from the moment the vector is applied, so a rotor that settles at once is
    // judged on the mean the settled samples could reach, not on the final current.
    float MotorAlignmentImpl::SettledShareOfFinalCurrent() const
    {
        const auto resistance = alignmentConfig.phaseResistance.Value();
        const auto inductance = alignmentConfig.phaseInductance.Value();
        const auto sampleRate = static_cast<float>(alignmentConfig.samplingFrequency.Value());

        if (resistance <= 0.0f || inductance <= 0.0f || sampleRate <= 0.0f || consecutiveSettledSamples == 0)
            return 1.0f;

        const auto samplesPerTimeConstant = sampleRate * inductance / resistance;
        const auto firstSample = settledEndSample + 1 - consecutiveSettledSamples;
        float share = 0.0f;

        for (auto sample = firstSample; sample <= settledEndSample; ++sample)
            share += 1.0f - std::exp(-static_cast<float>(sample) / samplesPerTimeConstant);

        return share / static_cast<float>(consecutiveSettledSamples);
    }

    void MotorAlignmentImpl::CalculateAlignmentOffset()
    {
        alignedPosition = encoder.Read();
        encoder.SetZero();
        driver.Stop();
        Complete(std::make_optional<foc::Radians>(alignedPosition));
    }

    void MotorAlignmentImpl::FailToConverge()
    {
        if (finishing)
            return;

        driver.Stop();
        Complete(std::nullopt);
    }

    // Interrupt context: only the stop runs here; the run tag drops an aborted run's outcome.
    void MotorAlignmentImpl::Finish(std::optional<foc::Radians> result)
    {
        driver.Stop();
        outcome = result;
        finishing = true;

        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [finishingRun = run](const infra::SharedPtr<MotorAlignmentImpl>& self)
            {
                if (self->run != finishingRun)
                    return;

                self->Complete(self->outcome);
            },
            WeakFromThis());
    }

    void MotorAlignmentImpl::Complete(std::optional<foc::Radians> result)
    {
        timeoutTimer.Cancel();

        if (onAlignmentDone)
            onAlignmentDone(result);
    }

    bool MotorAlignmentImpl::PhaseMissing() const
    {
        return phaseMissing;
    }

    void MotorAlignmentImpl::Abort()
    {
        if (!onAlignmentDone)
            return;

        timeoutTimer.Cancel();
        driver.Stop();
        onAlignmentDone = nullptr;
    }
}
