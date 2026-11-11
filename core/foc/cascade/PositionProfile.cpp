#include "core/foc/cascade/PositionProfile.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include <algorithm>

namespace foc
{
    PositionProfile::PositionProfile(hal::Hertz outerFrequency)
        : samplePeriod{ 1.0f / static_cast<float>(outerFrequency.Value()) }
    {}

    void PositionProfile::Limit(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration)
    {
        velocityLimit = velocity.Value();
        accelerationLimit = acceleration.Value();
        decelerationLimit = deceleration.Value() > 0.0f ? deceleration.Value() : acceleration.Value();
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

    PositionProfile::PlanLimits PositionProfile::CurrentLimits() const
    {
        return { velocityLimit, accelerationLimit, decelerationLimit };
    }

    ProfiledReference PositionProfile::Advance(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed)
    {
        if (!IsActive())
        {
            seeded = false;
            return { Radians{ target } };
        }

        if (!seeded || plannedSequence != targetSequence)
            Replan(travel, measured, measuredSpeed, CurrentLimits());

        elapsed = std::min(elapsed + samplePeriod, duration);
        const auto sample = plan.Sample(elapsed);
        position = sample.position;
        velocity = sample.velocity;
        reference = travel.IsOn() ? position : math::WrapToPi(position);
        seeded = true;

        return { Radians{ reference }, RadiansPerSecond{ velocity }, RadiansPerSecondSquared{ sample.acceleration } };
    }

    void PositionProfile::Replan(const TravelLimits& travel, Radians measured, RadiansPerSecond measuredSpeed, const PlanLimits& limits)
    {
        if (!seeded)
        {
            position = measured.Value();
            velocity = measuredSpeed.Value();
        }

        plannedSequence = targetSequence;
        const Radians point{ target };

        if (travel.IsOn())
            plan = Plan{ position, velocity, travel.Hold(point).Value(), limits };
        else
        {
            const auto start = math::WrapToPi(position);
            plan = Plan{ start, velocity, start + WrappedPositionError(point, Radians{ start }), limits };
        }

        elapsed = 0.0f;
        duration = plan.Duration();
    }
}
