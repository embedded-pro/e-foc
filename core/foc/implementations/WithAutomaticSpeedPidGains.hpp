#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    class WithAutomaticSpeedPidGains
    {
    public:
        explicit WithAutomaticSpeedPidGains(FocSpeedTunable& foc);

        void SetPidBasedOnInertiaAndFriction(Volts Vdc, NewtonMeterSecondSquared inertia, NewtonMeterSecondPerRadian friction, float bandwidthRadPerSec);

    private:
        FocSpeedTunable& foc;
    };
}
