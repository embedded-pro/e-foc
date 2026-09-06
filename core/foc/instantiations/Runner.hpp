#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"

namespace foc
{
    class Runner
        : public PhaseCurrentsObservable
    {
    public:
        Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocBase& foc);
        ~Runner() override;

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = delete;
        Runner& operator=(Runner&&) = delete;

        void Enable();
        void Disable();

        void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer) override;
        void UnregisterPhaseCurrentsObserver() override;

    private:
        void RegisterPhaseCurrents();
        void ReleasePhaseCurrents();
        void OnPhaseCurrents(const PhaseCurrents& currentPhases);

        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        FocBase& foc;
        infra::Function<void(const PhaseCurrents& currentPhases)> phaseCurrentsObserver;
        // Tested instead of the Function itself: comparing one against nullptr is an out-of-line
        // call, and this sits on the 20 kHz path where no observer is registered.
        volatile bool observerRegistered{ false };
        volatile bool enabled{ false };
    };
}
