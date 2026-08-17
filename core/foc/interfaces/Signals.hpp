#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"

namespace foc
{
    struct PhaseCurrents
    {
        Ampere a;
        Ampere b;
        Ampere c;
    };

    struct PhasePwmDutyCycles
    {
        hal::Percent a;
        hal::Percent b;
        hal::Percent c;
    };

    struct ThreePhase
    {
        float a;
        float b;
        float c;
    };

    struct TwoPhase
    {
        float alpha;
        float beta;
    };

    struct RotatingFrame
    {
        float d;
        float q;
    };

    enum class Direction : uint8_t
    {
        forward,
        reverse,
    };
}
