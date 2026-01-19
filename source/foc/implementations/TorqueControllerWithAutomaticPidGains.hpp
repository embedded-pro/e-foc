#pragma once

#include "foc/interfaces/Units.hpp"
#include "source/foc/implementations/TorqueControllerImpl.hpp"

namespace foc
{
    class TorqueControllerWithAutomaticPidGains
        : public TorqueControllerImpl
    {
    public:
        TorqueControllerWithAutomaticPidGains(MotorDriver& interface, Encoder& position, FieldOrientedControllerTorqueControl& foc);

        void SetPidBasedOnResistanceAndInductance(Volts Vdc, Ohm resistance, MilliHenry inductance, float nyquistFactor);
    };
}
