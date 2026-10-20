#include "core/services/mechanical_system_ident/MechanicalWindowKinematics.hpp"
#include <algorithm>
#include <cmath>

namespace services
{
    namespace
    {
        constexpr float accelerationQuantumTarget{ 100.0f };
        constexpr uint32_t minimumWindowsPerObservation{ 4 };
        constexpr uint32_t maximumWindowsPerObservation{ 16 };
        constexpr uint32_t minimumMovingWindows{ 8 };
    }

    MechanicalWindowKinematics::MechanicalWindowKinematics(float windowPeriod)
        : windowPeriod{ windowPeriod }
        , windowsPerObservation{ minimumWindowsPerObservation }
    {}

    std::optional<MechanicalObservation> MechanicalWindowKinematics::Add(const foc::MechanicalWindow& window)
    {
        if (window.first)
            Restart();

        TrackResolution(window.smallestStep.Value());
        Merge(window);

        if (merged < windowsPerObservation)
            return std::nullopt;

        return CloseObservation();
    }

    uint32_t MechanicalWindowKinematics::WindowsPerObservation() const
    {
        return windowsPerObservation;
    }

    void MechanicalWindowKinematics::Restart()
    {
        merged = 0;
        rotationSoFar = 0.0f;
        positionSum = 0.0f;
        iqSum = 0.0f;
        iqTimesUSum = 0.0f;
        iqTimesUSquaredSum = 0.0f;
        history = 0;
    }

    void MechanicalWindowKinematics::TrackResolution(float smallestStep)
    {
        if (smallestStep <= 0.0f)
            return;

        countAngle = movingWindows == 0 ? smallestStep : std::min(countAngle, smallestStep);
        movingWindows = std::min(movingWindows + 1, minimumMovingWindows);

        if (movingWindows < minimumMovingWindows)
            return;

        const auto seconds = std::sqrt(countAngle / accelerationQuantumTarget);
        const auto wanted = std::clamp(static_cast<uint32_t>(std::ceil(seconds / windowPeriod)), minimumWindowsPerObservation, maximumWindowsPerObservation);

        if (wanted == windowsPerObservation)
            return;

        windowsPerObservation = wanted;
        Restart();
    }

    void MechanicalWindowKinematics::Merge(const foc::MechanicalWindow& window)
    {
        const auto index = static_cast<float>(merged);
        const auto iq = window.meanIq.Value();

        positionSum += rotationSoFar + window.meanPosition.Value();
        rotationSoFar += window.rotation.Value();
        iqSum += iq;
        iqTimesUSum += index * iq + window.meanIqTimesU;
        iqTimesUSquaredSum += index * index * iq + 2.0f * index * window.meanIqTimesU + window.meanIqTimesUSquared;
        ++merged;
    }

    std::optional<MechanicalObservation> MechanicalWindowKinematics::CloseObservation()
    {
        const auto windows = static_cast<float>(windowsPerObservation);
        const auto period = windowPeriod * windows;

        const auto meanOffset = positionSum / windows;
        const auto meanIq = iqSum / windows;
        const auto meanIqTimesU = iqTimesUSum / (windows * windows);
        const auto meanIqTimesUSquared = iqTimesUSquaredSum / (windows * windows * windows);

        const auto rise = 0.5f * meanIqTimesUSquared;
        const auto peak = 0.5f * meanIq + meanIqTimesU - meanIqTimesUSquared;
        const auto fall = 0.5f * (meanIq - 2.0f * meanIqTimesU + meanIqTimesUSquared);
        const auto speed = (previousMeanToEnd + meanOffset) / period;

        std::optional<MechanicalObservation> observation;
        if (history == 2)
            observation = MechanicalObservation{ (speed - previousSpeed) / period, 0.5f * (speed + previousSpeed), earlierRise + previousPeak + fall };

        previousMeanToEnd = rotationSoFar - meanOffset;
        previousSpeed = speed;
        earlierRise = previousRise;
        previousRise = rise;
        previousPeak = peak;
        history = std::min(history + 1, uint32_t{ 2 });

        merged = 0;
        rotationSoFar = 0.0f;
        positionSum = 0.0f;
        iqSum = 0.0f;
        iqTimesUSum = 0.0f;
        iqTimesUSquaredSum = 0.0f;

        return observation;
    }
}
