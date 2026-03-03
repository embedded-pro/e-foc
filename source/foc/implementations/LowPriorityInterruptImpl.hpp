#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "source/foc/interfaces/Driver.hpp"

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
