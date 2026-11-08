#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include <algorithm>
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
        settledSumA = 0.0f;
        settledSumB = 0.0f;
        settledSumC = 0.0f;
        failure = Failure::none;
        startingCurrents = {};
        firstHold = config.followStep.has_value();
        samplesInHold = 0;
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
        driver.Stop();
        OutputVector(firstHold ? alignmentAngle + alignmentConfig.followStep->Value() : alignmentAngle);

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

    void MotorAlignmentImpl::OutputVector(float electricalAngle)
    {
        auto voltage = foc::DutyFraction(alignmentConfig.testVoltage);

        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ voltage, 0.0f }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle))));
    }

    void MotorAlignmentImpl::ProcessPosition(const foc::PhaseCurrents& currents)
    {
        auto currentPosition = encoder.Read();
        auto positionChange = std::abs(math::WrapToPi((currentPosition - previousPosition).Value()));
        previousPosition = currentPosition;
        ++samplesInHold;

        if (positionChange < alignmentConfig.settledThreshold.Value() && CountsSettlement())
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
            settledSumA = 0.0f;
            settledSumB = 0.0f;
            settledSumC = 0.0f;
        }
    }

    bool MotorAlignmentImpl::CountsSettlement() const
    {
        return firstHold || !alignmentConfig.followStep.has_value() || samplesInHold > alignmentConfig.followDwell;
    }

    void MotorAlignmentImpl::Settle()
    {
        if (firstHold)
        {
            BeginFinalHold();
            return;
        }

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
        if (CarriesNoCurrent())
            failure = Failure::phaseMissing;
        else if (alignmentConfig.followStep.has_value() && !FollowedField())
            failure = Failure::encoderNotFollowing;
        else
        {
            CalculateAlignmentOffset();
            return;
        }

        driver.Stop();
        Complete(std::nullopt);
    }

    void MotorAlignmentImpl::BeginFinalHold()
    {
        firstHold = false;
        firstHoldPosition = previousPosition;
        const auto held = static_cast<float>(std::max<std::size_t>(consecutiveSettledSamples, 1));
        startingCurrents = { settledSumA / held, settledSumB / held, settledSumC / held };
        currentSampleIndex = 0;
        samplesInHold = 0;
        consecutiveSettledSamples = 0;
        settledSumA = 0.0f;
        settledSumB = 0.0f;
        settledSumC = 0.0f;
        OutputVector(alignmentAngle);
    }

    bool MotorAlignmentImpl::FollowedField() const
    {
        const auto travel = math::WrapToPi((previousPosition - firstHoldPosition).Value());
        const auto expected = -alignmentConfig.followStep->Value() / static_cast<float>(std::max<std::size_t>(polePairs, 1));
        const auto ratio = travel / expected;

        return ratio >= minimumFollow && ratio <= maximumFollow;
    }

    bool MotorAlignmentImpl::CarriesNoCurrent() const
    {
        const auto samples = static_cast<float>(consecutiveSettledSamples);
        const auto a = settledSumA / samples;
        const auto b = settledSumB / samples;
        const auto c = settledSumC / samples;
        const auto finalCurrent = ExpectedPhaseACurrent();

        if (!finalCurrent.has_value())
        {
            const auto floor = missingFraction * 0.5f * std::abs(a);
            return std::abs(b) < floor || std::abs(c) < floor;
        }

        const auto risen = SettledShareOfFinalCurrent();

        return Missing(a, *finalCurrent, startingCurrents.a, risen) ||
               Missing(b, -0.5f * *finalCurrent, startingCurrents.b, risen) ||
               Missing(c, -0.5f * *finalCurrent, startingCurrents.c, risen);
    }

    bool MotorAlignmentImpl::Missing(float measured, float finalCurrent, float starting, float risen) const
    {
        const auto expected = finalCurrent * risen + starting * (1.0f - risen);
        const auto crossing = starting * finalCurrent < 0.0f;

        if (crossing && std::abs(expected) < missingFraction * std::abs(finalCurrent))
            return false;

        return std::abs(measured) < missingFraction * std::abs(expected);
    }

    std::optional<float> MotorAlignmentImpl::ExpectedPhaseACurrent() const
    {
        const auto resistance = alignmentConfig.phaseResistance.Value();
        const auto bus = alignmentConfig.busVoltage.Value();

        if (resistance <= 0.0f || bus <= 0.0f)
            return std::nullopt;

        return 0.5f * foc::DutyFraction(alignmentConfig.testVoltage) * bus / resistance;
    }

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

    MotorAlignment::Failure MotorAlignmentImpl::LastFailure() const
    {
        return failure;
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
