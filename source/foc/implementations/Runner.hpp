#pragma once

#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Foc.hpp"

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
