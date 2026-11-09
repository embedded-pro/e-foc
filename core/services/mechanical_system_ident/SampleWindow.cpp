#include "core/services/mechanical_system_ident/SampleWindow.hpp"
#include "numerical/math/AngleUtilities.hpp"

namespace services
{
    namespace
    {
        constexpr std::size_t windowHysteresisFloor{ 16 };
        constexpr std::size_t windowHysteresisDivisor{ 16 };
    }

    void SampleWindow::Restart(float position, float samplingPeriod)
    {
        resolution.Restart(position);
        this->samplingPeriod = samplingPeriod;
        windowSamples = 1;
        samplesInWindow = 0;
        windowPeriod = samplingPeriod;
        samplesSinceStart = 0;
        previousPosition = position;
        previousSpeed = 0.0f;
        hasPreviousMean = false;
        hasPreviousSpeed = false;
        positionSum = 0.0f;
        previousMeanToEnd = 0.0f;
        torque = TorqueKernel{};
    }

    std::optional<ClosedWindow> SampleWindow::Add(float position, float sampleTorque)
    {
        ++samplesSinceStart;
        Accumulate(position, sampleTorque);
        resolution.Sample(position);

        if (++samplesInWindow < windowSamples)
            return std::nullopt;

        samplesInWindow = 0;
        const auto closed = Close(position);
        if (Resize())
            return std::nullopt;

        return closed;
    }

    void SampleWindow::Accumulate(float position, float sampleTorque)
    {
        const auto weight = (static_cast<float>(samplesInWindow) + 0.5f) / static_cast<float>(windowSamples);
        torque.sum += sampleTorque;
        torque.rise += sampleTorque * 0.5f * weight * weight;
        torque.peak += sampleTorque * (0.5f + weight * (1.0f - weight));
        torque.fall += sampleTorque * 0.5f * (1.0f - weight) * (1.0f - weight);
        positionSum += math::WrapToPi(position - previousPosition);
    }

    ClosedWindow SampleWindow::Close(float position)
    {
        const auto samples = static_cast<float>(windowSamples);
        const auto window = torque;
        const auto meanOffset = positionSum / samples;
        torque = TorqueKernel{};
        positionSum = 0.0f;

        const auto speed = (previousMeanToEnd + meanOffset) / windowPeriod;
        const auto centred = windowSamples > 1;
        const ClosedWindow closed{
            .speed = speed,
            .acceleration = (speed - previousSpeed) / windowPeriod,
            .centredSpeed = centred ? 0.5f * (speed + previousSpeed) : speed,
            .centredTorque = centred ? (earlierTorqueRise + previousTorquePeak + window.fall) / samples : window.sum / samples,
            .hadMean = hasPreviousMean,
            .hadSpeed = hasPreviousSpeed,
            .resolutionKnown = resolution.Known(),
            .sample = static_cast<uint32_t>(samplesSinceStart),
            .windowSamples = static_cast<uint32_t>(windowSamples),
        };

        previousMeanToEnd = math::WrapToPi(position - previousPosition) - meanOffset;
        previousPosition = position;
        previousSpeed = speed;
        hasPreviousSpeed = closed.hadMean;
        hasPreviousMean = true;
        earlierTorqueRise = previousTorqueRise;
        previousTorqueRise = window.rise;
        previousTorquePeak = window.peak;
        return closed;
    }

    bool SampleWindow::Resize()
    {
        const auto wanted = resolution.Known() ? resolution.WindowSamples(samplingPeriod) : windowSamples;
        const auto difference = wanted > windowSamples ? wanted - windowSamples : windowSamples - wanted;
        if (difference == 0 || (windowSamples >= windowHysteresisFloor && difference * windowHysteresisDivisor <= windowSamples))
            return false;

        windowSamples = wanted;
        windowPeriod = samplingPeriod * static_cast<float>(wanted);
        hasPreviousMean = false;
        hasPreviousSpeed = false;
        return true;
    }
}
