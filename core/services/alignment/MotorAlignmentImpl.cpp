#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
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
        onAlignmentDone = onDone;
        currentSampleIndex = 0;
        consecutiveSettledSamples = 0;
        previousPosition = encoder.Read();

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
                if (!onAlignmentDone)
                    return;

                if (ExceedsInjectionLimit(currents, driver.MaxCurrentSupported()))
                {
                    FailToConverge();
                    return;
                }

                currentSampleIndex++;

                if (currentSampleIndex >= alignmentConfig.maxSamples)
                    FailToConverge();
                else
                    ProcessPosition();
            });
    }

    void MotorAlignmentImpl::ProcessPosition()
    {
        auto currentPosition = encoder.Read();
        auto positionChange = std::abs((currentPosition - previousPosition).Value());

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

        timeoutTimer.Cancel();
        driver.Stop();

        if (onAlignmentDone)
            onAlignmentDone(std::make_optional<foc::Radians>(alignedPosition));
    }

    void MotorAlignmentImpl::FailToConverge()
    {
        timeoutTimer.Cancel();
        driver.Stop();

        if (onAlignmentDone)
            onAlignmentDone(std::nullopt);
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
