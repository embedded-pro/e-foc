#include "core/foc/cascade/PositionProfile.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include <algorithm>

namespace foc
{
    PositionProfile::PositionProfile(hal::Hertz outerFrequency)
        : samplePeriod{ 1.0f / static_cast<float>(outerFrequency.Value()) }
    {}

    void PositionProfile::Limit(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration, RadiansPerSecondCubed jerk)
    {
        velocityLimit = velocity.Value();
        accelerationLimit = acceleration.Value();
        decelerationLimit = deceleration.Value() > 0.0f ? deceleration.Value() : acceleration.Value();
        jerkLimit = jerk.Value();
    }

    void PositionProfile::SetTarget(Radians point)
    {
        target = point.Value();
        targetSequence = targetSequence + 1;
    }

    void PositionProfile::Release()
    {
        seeded = false;
    }

    Radians PositionProfile::Target() const
    {
        return Radians{ target };
    }

    Radians PositionProfile::Reference() const
    {
        return seeded ? Radians{ reference } : Radians{ target };
    }

    bool PositionProfile::IsActive() const
    {
        return velocityLimit > 0.0f && accelerationLimit > 0.0f;
    }

    bool PositionProfile::IsScurve() const
    {
        return jerkLimit > 0.0f;
    }

    PositionProfile::TrapezoidalLimits PositionProfile::TrapezoidalCurrentLimits() const
    {
        return { velocityLimit, accelerationLimit, decelerationLimit };
    }

    PositionProfile::SCurveLimits PositionProfile::SCurveCurrentLimits() const
    {
        return { velocityLimit, accelerationLimit, decelerationLimit, jerkLimit };
    }

    float PositionProfile::ClampedAcceleration() const
    {
        const float aMax = std::max(accelerationLimit, decelerationLimit);
        return std::clamp(acceleration, -aMax, aMax);
    }

    ProfiledReference PositionProfile::Advance(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed)
    {
        if (!IsActive())
        {
            seeded = false;
            return { Radians{ target } };
        }

        if (!seeded || plannedSequence != targetSequence)
            Replan(travel, measured, measuredSpeed);

        elapsed = std::min(elapsed + samplePeriod, duration);
        const auto sample = std::visit([this](const auto& p)
            {
                return p.Sample(elapsed);
            },
            plan);
        position = sample.position;
        velocity = sample.velocity;
        acceleration = sample.acceleration;
        reference = travel.Folds() ? math::WrapToPi(position) : position;
        seeded = true;

        return { Radians{ reference }, RadiansPerSecond{ velocity }, RadiansPerSecondSquared{ acceleration } };
    }

    float PositionProfile::Goal(const TravelLimits& travel, Radians point, float start)
    {
        if (travel.IsOn())
            return travel.Hold(point).Value();

        if (travel.Folds())
            return start + WrappedPositionError(point, Radians{ start });

        return point.Value();
    }

    void PositionProfile::Replan(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed)
    {
        if (!seeded)
        {
            position = measured.Value();
            velocity = measuredSpeed.Value();
            acceleration = 0.0f;
        }

        plannedSequence = targetSequence;
        const Radians point{ target };

        const auto start = travel.Folds() ? math::WrapToPi(position) : position;
        const auto goal = Goal(travel, point, start);

        if (IsScurve())
            plan.emplace<SCurvePlan>(start, velocity, ClampedAcceleration(), goal, SCurveCurrentLimits());
        else
            plan.emplace<TrapezoidalPlan>(start, velocity, goal, TrapezoidalCurrentLimits());

        elapsed = 0.0f;
        duration = std::visit([](const auto& p)
            {
                return p.Duration();
            },
            plan);
    }
}
