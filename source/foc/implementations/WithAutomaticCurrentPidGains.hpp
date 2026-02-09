#pragma once

#include "source/foc/interfaces/Foc.hpp"
#include <numbers>

namespace foc
{
    class WithAutomaticCurrentPidGains
    {
    public:
        explicit WithAutomaticCurrentPidGains(FocBase& foc);

        void SetPidBasedOnResistanceAndInductance(Volts Vdc, Ohm resistance, MilliHenry inductance, hal::Hertz baseFrequency, float nyquistFactor);

    private:
        FocBase& foc;
    };
}
