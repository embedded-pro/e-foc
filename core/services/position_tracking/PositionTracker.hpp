#pragma once

#include "core/foc/cascade/TurnHandover.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/timer/Timer.hpp"

namespace services
{
    // The encoder port of the services that set the position zero: a new zero restarts the turn count.
    class PositionTracker
        : public drivers::Encoder
    {
    public:
        static constexpr infra::Duration samplePeriod{ std::chrono::milliseconds(1) };

        explicit PositionTracker(drivers::Encoder& encoder);

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        foc::Radians Position() const;
        foc::TurnHandover& Handover();

    private:
        drivers::Encoder& encoder;
        foc::TurnHandover handover;
        infra::TimerRepeating sampleTimer;
    };
}
