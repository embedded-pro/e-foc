#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace foc
{
    class LowPriorityInterruptImpl
        : public LowPriorityInterrupt
    {
    public:
        void Register(const infra::Function<void()>& handler) override
        {
            this->handler = handler;
        }

        void Trigger() override
        {
            if (handler)
                infra::EventDispatcherWithWeakPtr::Instance().Schedule([this]()
                    {
                        handler();
                    });
        }

    private:
        infra::Function<void()> handler;
    };
}
