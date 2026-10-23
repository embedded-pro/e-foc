#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "infra/util/Function.hpp"

namespace foc
{
    class LowPriorityInterrupt
    {
    protected:
        ~LowPriorityInterrupt() = default;

    public:
        virtual void Trigger() = 0;
        virtual void Register(const infra::Function<void()>& handler) = 0;
        virtual void Unregister() = 0;
    };

    class Controllable
    {
    protected:
        ~Controllable() = default;

    public:
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };

    class PhaseCurrentsObservable
    {
    protected:
        ~PhaseCurrentsObservable() = default;

    public:
        virtual void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer) = 0;
        virtual void UnregisterPhaseCurrentsObserver() = 0;
    };
}
