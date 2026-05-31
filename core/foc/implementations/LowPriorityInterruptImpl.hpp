#pragma once

#include "core/foc/interfaces/Driver.hpp"
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
