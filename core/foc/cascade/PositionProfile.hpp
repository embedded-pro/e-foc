#pragma once

#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/math/TravelLimits.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "numerical/motion_profiles/SCurveProfileFromState.hpp"
#include "numerical/motion_profiles/TrapezoidalProfileFromState.hpp"
#include <cstdint>
#include <variant>

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

        void Limit(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration, RadiansPerSecondCubed jerk = RadiansPerSecondCubed{ 0.0f });
        void SetTarget(Radians point);
        void Release();

        Radians Target() const;
        Radians Reference() const;
        bool IsActive() const;
        ProfiledReference Advance(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed);

    private:
        using TrapezoidalPlan = motion_profiles::TrapezoidalProfileFromState<float>;
        using SCurvePlan = motion_profiles::SCurveProfileFromState<float>;
        using TrapezoidalLimits = motion_profiles::AsymmetricMotionLimits<float>;
        using SCurveLimits = motion_profiles::JerkLimitedMotionLimits<float>;
        using PlanVariant = std::variant<TrapezoidalPlan, SCurvePlan>;

        bool IsScurve() const;
        TrapezoidalLimits TrapezoidalCurrentLimits() const;
        SCurveLimits SCurveCurrentLimits() const;
        float ClampedAcceleration() const;
        void Replan(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed);

        float samplePeriod;
        // Written by the event loop, read by the outer-loop tick that replans on a changed sequence.
        volatile float target{ 0.0f };
        volatile uint32_t targetSequence{ 0 };
        volatile float velocityLimit{ 0.0f };
        volatile float accelerationLimit{ 0.0f };
        volatile float decelerationLimit{ 0.0f };
        volatile float jerkLimit{ 0.0f };
        volatile float reference{ 0.0f };
        volatile bool seeded{ false };
        uint32_t plannedSequence{ 0 };
        float position{ 0.0f };
        float velocity{ 0.0f };
        float acceleration{ 0.0f };
        float elapsed{ 0.0f };
        float duration{ 0.0f };
        PlanVariant plan{ std::in_place_type<TrapezoidalPlan>, 0.0f, 0.0f, 0.0f, TrapezoidalLimits{ 1.0f, 1.0f, 1.0f } };
    };
}
