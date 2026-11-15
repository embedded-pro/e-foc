#include "core/services/position_tracking/PositionTracker.hpp"

namespace services
{
    PositionTracker::PositionTracker(drivers::Encoder& encoder)
        : encoder{ encoder }
        , sampleTimer{ samplePeriod, [this]()
            {
                handover.Track(this->encoder.Read().Value());
            } }
    {}

    foc::Radians PositionTracker::Read()
    {
        return encoder.Read();
    }

    void PositionTracker::Set(foc::Radians value)
    {
        encoder.Set(value);
        handover.Restart();
    }

    void PositionTracker::SetZero()
    {
        encoder.SetZero();
        handover.Restart();
    }

    foc::Radians PositionTracker::Position() const
    {
        return handover.Position();
    }

    foc::TurnHandover& PositionTracker::Handover()
    {
        return handover;
    }
}
