#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
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
            transforms.Inverse(foc::RotatingFrame{ voltage, 0.0f }, std::cos(electricalAngle), std::sin(electricalAngle))));

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
                    ProcessPosition();
            });
    }

    void MotorAlignmentImpl::ProcessPosition()
    {
        auto currentPosition = encoder.Read();
        auto positionChange = std::abs(foc::detail::PositionWithWrapAround((currentPosition - previousPosition).Value()));

        if (positionChange < alignmentConfig.settledThreshold.Value())
        {
            consecutiveSettledSamples++;
            if (consecutiveSettledSamples >= alignmentConfig.settledCount)
                CalculateAlignmentOffset();
        }
        else
            consecutiveSettledSamples = 0;

        previousPosition = currentPosition;
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

    // Interrupt context: the caller's completion is event-loop work, and the run tag drops an aborted run's outcome.
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

    void MotorAlignmentImpl::Abort()
    {
        if (!onAlignmentDone)
            return;

        timeoutTimer.Cancel();
        driver.Stop();
        onAlignmentDone = nullptr;
    }
}
