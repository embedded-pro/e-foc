#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "infra/util/Function.hpp"

namespace foc
{
    class LowPriorityInterrupt
    {
    public:
        virtual ~LowPriorityInterrupt() = default;

        virtual void Trigger() = 0;
        virtual void Register(const infra::Function<void()>& handler) = 0;
        virtual void Unregister() = 0;
    };

    class Controllable
    {
    public:
        virtual ~Controllable() = default;

        virtual void Start() = 0;
        virtual void Stop() = 0;
    };

    // Lets a calibration service watch the phase currents the control loop is already sampling,
    // instead of taking ThreePhaseInverter::PhaseCurrentsReady away from it. The observer runs in
    // the ADC interrupt after the duty cycles have been written, so it adds no jitter to the write.
    class PhaseCurrentsObservable
    {
    public:
        virtual ~PhaseCurrentsObservable() = default;

        virtual void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer) = 0;
        virtual void UnregisterPhaseCurrentsObserver() = 0;
    };
}
