#pragma once

#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/TravelLimits.hpp"
#include "numerical/motion_profiles/TrapezoidalProfileFromState.hpp"
#include <cstdint>

namespace foc
{
    struct ProfiledReference
    {
        Radians position{ 0.0f };
        RadiansPerSecond velocity{ 0.0f };
        RadiansPerSecondSquared acceleration{ 0.0f };
    };

    class PositionProfile
    {
    public:
        explicit PositionProfile(hal::Hertz outerFrequency);

        void Limit(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration);
        void SetTarget(Radians point);
        void Release();

        Radians Target() const;
        Radians Reference() const;
        bool IsActive() const;
        ProfiledReference Advance(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed);

    private:
        using Plan = motion_profiles::TrapezoidalProfileFromState<float>;
        using PlanLimits = motion_profiles::AsymmetricMotionLimits<float>;

        PlanLimits CurrentLimits() const;
        void Replan(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed, const PlanLimits& limits);

        float samplePeriod;
        // Written by the event loop, read by the outer-loop tick that replans on a changed sequence.
        volatile float target{ 0.0f };
        volatile uint32_t targetSequence{ 0 };
        volatile float velocityLimit{ 0.0f };
        volatile float accelerationLimit{ 0.0f };
        volatile float decelerationLimit{ 0.0f };
        volatile float reference{ 0.0f };
        volatile bool seeded{ false };
        uint32_t plannedSequence{ 0 };
        float position{ 0.0f };
        float velocity{ 0.0f };
        float elapsed{ 0.0f };
        float duration{ 0.0f };
        Plan plan{ 0.0f, 0.0f, 0.0f, PlanLimits{ 1.0f, 1.0f, 1.0f } };
    };
}
