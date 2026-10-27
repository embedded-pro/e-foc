#pragma once

#include "core/foc/interfaces/OnlineEstimators.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    struct MechanicalObservation
    {
        float acceleration;
        float speed;
        float iq;
    };

    // Merges outer-loop windows into observations; see Window Kinematics in service-mechanical-ident.md.
    class MechanicalWindowKinematics
    {
    public:
        explicit MechanicalWindowKinematics(float windowPeriod);

        std::optional<MechanicalObservation> Add(const foc::MechanicalWindow& window);
        uint32_t WindowsPerObservation() const;
        void Restart();

    private:
        void TrackResolution(float smallestStep);
        void Merge(const foc::MechanicalWindow& window);
        std::optional<MechanicalObservation> CloseObservation();

        float windowPeriod;
        uint32_t windowsPerObservation;
        float countAngle{ 0.0f };
        uint32_t movingWindows{ 0 };

        uint32_t merged{ 0 };
        float rotationSoFar{ 0.0f };
        float positionSum{ 0.0f };
        float iqSum{ 0.0f };
        float iqTimesUSum{ 0.0f };
        float iqTimesUSquaredSum{ 0.0f };

        uint32_t history{ 0 };
        float previousMeanToEnd{ 0.0f };
        float previousSpeed{ 0.0f };
        float earlierRise{ 0.0f };
        float previousRise{ 0.0f };
        float previousPeak{ 0.0f };
    };
}
