#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <cstddef>

namespace foc
{
    struct MotorModelParameters
    {
        Ohm resistance{ 0.0f };
        MilliHenry inductance{ 0.0f };
        Weber fluxLinkage{ 0.0f };
        Volts busVoltage{ 0.0f };
        hal::Hertz samplingFrequency{ 0 };
        std::size_t polePairs{ 0 };
    };

    struct MechanicalModelParameters
    {
        NewtonMeterSecondSquared inertia{ 0.0f };
        NewtonMeterSecondPerRadian viscousFriction{ 0.0f };
        NewtonMeter torqueConstant{ 0.0f };
        Ampere maxCurrent{ 0.0f };
        hal::Hertz samplingFrequency{ 0 };
    };

    // Amplitude-invariant Clarke: T = 3/2 · p · ψf · iq, the factor the plant model applies too
    inline NewtonMeter TorqueConstantFor(std::size_t polePairs, Weber fluxLinkage)
    {
        return NewtonMeter{ 1.5f * static_cast<float>(polePairs) * fluxLinkage.Value() };
    }
}
