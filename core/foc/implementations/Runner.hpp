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

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = delete;
        Runner& operator=(Runner&&) = delete;

        void Enable();
        void Disable();

    private:
        ThreePhaseInverter& inverter;
        Encoder& encoder;
        FocBase& foc;
    };
}
