#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace application
{
    // The weak-pointer dispatcher is a second singleton next to the plain one. A platform that registers
    // only the plain cortex dispatcher leaves it unset, and every completion scheduled against a WeakPtr
    // (mechanical identification, deferred fault notifications) dereferences nothing. This worker
    // registers both and idles the core the way the plain cortex worker does.
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

    using EventDispatcherCortexWithWeakPtr = infra::EventDispatcherWithWeakPtrConnector<EventDispatcherCortexWithWeakPtrWorker>;
}
