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
        hal::DutyCycle a;
        hal::DutyCycle b;
        hal::DutyCycle c;
    };

    inline float DutyFraction(hal::DutyCycle duty)
    {
        return static_cast<float>(duty.Value()) * (1.0f / static_cast<float>(hal::DutyCycle::fullScale));
    }

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
