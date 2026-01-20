#include "source/services/alignment/MotorAlignmentImpl.hpp"
#include <cmath>

namespace
{
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
    MotorAlignmentImpl::MotorAlignmentImpl(foc::MotorDriver& driver, foc::Encoder& encoder)
        : driver(driver)
        , encoder(encoder)
    {
    }

    void MotorAlignmentImpl::ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone)
    {
        if (onAlignmentDone)
            return;

        this->polePairs = polePairs;
        alignmentConfig = config;
        onAlignmentDone = onDone;
        currentSampleIndex = 0;
        consecutiveSettledSamples = 0;
        previousPosition = encoder.Read();

        ApplyAlignmentVoltage();
    }

    void MotorAlignmentImpl::ApplyAlignmentVoltage()
    {
        auto voltage = static_cast<float>(alignmentConfig.testVoltagePercent.Value()) / 100.0f;
        auto electricalAngle = alignmentAngle;

        driver.Stop();
        driver.ThreePhasePwmOutput(NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ voltage, 0.0f }, std::cos(electricalAngle), std::sin(electricalAngle))));

        driver.PhaseCurrentsReady(alignmentConfig.samplingFrequency, [this](auto)
            {
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
        driver.Stop();
        alignedPosition = encoder.Read();

        if (onAlignmentDone)
        {
            auto offset = foc::Radians{ alignedPosition.Value() * static_cast<float>(polePairs) };
            onAlignmentDone(std::make_optional<foc::Radians>(offset));
        }
    }

    void MotorAlignmentImpl::FailToConverge()
    {
        driver.Stop();

        if (onAlignmentDone)
            onAlignmentDone(std::nullopt);
    }
}
