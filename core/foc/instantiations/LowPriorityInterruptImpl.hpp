#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/util/SharedPtr.hpp"

namespace foc
{
    class LowPriorityInterruptImpl
        : public LowPriorityInterrupt
        , public infra::EnableSharedFromThis<LowPriorityInterruptImpl>
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
                infra::EventDispatcherWithWeakPtr::Instance().Schedule(
                    [](const infra::SharedPtr<LowPriorityInterruptImpl>& self)
                    {
                        if (self->handler)
                            self->handler();
                    },
                    WeakFromThis());
        }

    private:
        infra::Function<void()> handler;
    };
}
