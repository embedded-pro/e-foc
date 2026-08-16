#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"

namespace foc
{
    struct MotorModelParameters
    {
        Ohm resistance{ 0.0f };
        MilliHenry inductance{ 0.0f };
        Weber fluxLinkage{ 0.0f };
        Volts busVoltage{ 0.0f };
        hal::Hertz samplingFrequency{ 0 };
    };

    struct MechanicalModelParameters
    {
        NewtonMeterSecondSquared inertia{ 0.0f };
        NewtonMeterSecondPerRadian viscousFriction{ 0.0f };
        NewtonMeter torqueConstant{ 0.0f };
        Ampere maxCurrent{ 0.0f };
        hal::Hertz samplingFrequency{ 0 };
    };
}
