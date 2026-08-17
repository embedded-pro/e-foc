#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"

namespace foc
{
    class Runner
    {
    public:
        Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocBase& foc);
        ~Runner();

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = delete;
        Runner& operator=(Runner&&) = delete;

        void Enable();
        void Disable();

    private:
        void RegisterPhaseCurrents();
        void OnPhaseCurrents(const PhaseCurrents& currentPhases);

        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        FocBase& foc;
        bool enabled{ false };
    };
}
