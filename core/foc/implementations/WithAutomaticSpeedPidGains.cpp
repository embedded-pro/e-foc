#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"

namespace foc
{
    WithAutomaticSpeedPidGains::WithAutomaticSpeedPidGains(FocSpeedTunable& foc)
        : foc{ foc }
    {}

    void WithAutomaticSpeedPidGains::SetPidBasedOnInertiaAndFriction(
        Volts Vdc,
        NewtonMeterSecondSquared inertia,
        NewtonMeterSecondPerRadian friction,
        float bandwidthRadPerSec)
    {
        auto kp = inertia.Value() * bandwidthRadPerSec;
        auto ki = friction.Value() * bandwidthRadPerSec;

        foc.SetSpeedTunings(Vdc, SpeedTunings{ kp, ki, 0.0f });
    }
}
