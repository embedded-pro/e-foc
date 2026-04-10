#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Foc.hpp"

namespace foc
{
    class Runner
    {
    public:
        Runner(ThreePhaseInverter& inverter, Encoder& encoder, FocBase& foc);
        ~Runner();

        void Enable();
        void Disable();

    private:
        ThreePhaseInverter& inverter;
        Encoder& encoder;
        FocBase& foc;
    };
}
