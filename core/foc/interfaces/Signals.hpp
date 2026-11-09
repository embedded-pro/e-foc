#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <chrono>

namespace foc
{
    struct PhaseCurrents
    {
        Ampere a;
        Ampere b;
        Ampere c;
    };

    // Rate of each phase current while all low sides conduct (the zero vector)
    struct PhaseCurrentSlopes
    {
        AmperePerSecond a;
        AmperePerSecond b;
        AmperePerSecond c;
    };

    // Hold instant of each phase after the centre of the low-side window
    struct PhaseSampleDelays
    {
        std::chrono::duration<float> a;
        std::chrono::duration<float> b;
        std::chrono::duration<float> c;
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
}
