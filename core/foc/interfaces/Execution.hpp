#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "infra/util/Function.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <atomic>

namespace foc
{
    class ProgressSignal
    {
    public:
        // Signal is callable from any context including interrupts; TakeProgress runs on the event loop only
        ALWAYS_INLINE_HOT void Signal()
        {
            progressed.store(true);
        }

        bool TakeProgress()
        {
            return progressed.exchange(false);
        }

    private:
        std::atomic<bool> progressed{ false };
    };

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

    class PhaseCurrentsObservable
    {
    public:
        virtual ~PhaseCurrentsObservable() = default;

        virtual void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer) = 0;
        virtual void UnregisterPhaseCurrentsObserver() = 0;
    };
}
