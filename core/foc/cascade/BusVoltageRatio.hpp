#pragma once

#include "e-foc-protocol/foc/Units.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class BusVoltageRatio
    {
    public:
        void Configure(Volts configured);
        void Track(Volts measured);

        OPTIMIZE_FOR_SPEED float Ratio() const
        {
            return ratio;
        }

        OPTIMIZE_FOR_SPEED float Inverse() const
        {
            return inverse;
        }

    private:
        float configured{ 0.0f };
        // Written by the event loop, read by the control interrupt; a period may pair a new ratio with the old inverse.
        volatile float ratio{ 1.0f };
        volatile float inverse{ 1.0f };
    };
}
