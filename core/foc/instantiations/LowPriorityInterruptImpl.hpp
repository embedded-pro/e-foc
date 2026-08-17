#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace foc
{
    class LowPriorityInterruptImpl
        : public LowPriorityInterrupt
    {
    public:
        void Register(const infra::Function<void()>& _handler) override
        {
            this->handler = _handler;
        }

        void Unregister() override
        {
            handler = nullptr;
        }

        void Trigger() override
        {
            if (handler)
                infra::EventDispatcherWithWeakPtr::Instance().Schedule([this]()
                    {
                        // Re-checked here because Unregister may run after this event was queued
                        if (handler)
                            handler();
                    });
        }

    private:
        infra::Function<void()> handler;
    };
}
