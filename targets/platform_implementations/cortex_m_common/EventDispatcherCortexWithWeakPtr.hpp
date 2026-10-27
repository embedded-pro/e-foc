#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace application
{
    // Also registers the WeakPtr dispatcher singleton, which the plain cortex worker leaves unset.
    class EventDispatcherCortexWithWeakPtrWorker
        : public infra::EventDispatcherWithWeakPtrWorker
    {
    public:
        using infra::EventDispatcherWithWeakPtrWorker::EventDispatcherWithWeakPtrWorker;

    protected:
        void Idle() override
        {
            __asm volatile("dsb" ::: "memory");
            __asm volatile("wfe");
        }

        void RequestExecution() override
        {
            __asm volatile("dsb" ::: "memory");
            __asm volatile("sev");
        }
    };
}
