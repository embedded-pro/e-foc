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

    void MotorAlignmentImpl::Settle()
    {
        if (!CarriesNoCurrent())
        {
            CalculateAlignmentOffset();
            return;
        }

        phaseMissing = true;
        Finish(std::nullopt);
    }

    bool MotorAlignmentImpl::CarriesNoCurrent() const
    {
        const auto samples = static_cast<float>(consecutiveSettledSamples);
        const auto a = std::abs(settledSumA) / samples;
        const auto b = std::abs(settledSumB) / samples;
        const auto c = std::abs(settledSumC) / samples;
        const auto expected = ExpectedPhaseACurrent();

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

    void MotorAlignmentImpl::CalculateAlignmentOffset()
    {
        alignedPosition = encoder.Read();
        encoder.SetZero();
        Finish(std::make_optional<foc::Radians>(alignedPosition));
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
