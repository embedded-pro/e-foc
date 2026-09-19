#include "core/supervision/SupervisedLowPriorityInterrupt.hpp"

namespace supervision
{
    SupervisedLowPriorityInterrupt::SupervisedLowPriorityInterrupt(foc::LowPriorityInterrupt& lowPriorityInterrupt, foc::ProgressSignal& progress)
        : lowPriorityInterrupt{ lowPriorityInterrupt }
        , progress{ progress }
    {}

    void SupervisedLowPriorityInterrupt::Trigger()
    {
        lowPriorityInterrupt.Trigger();
    }

    void SupervisedLowPriorityInterrupt::Register(const infra::Function<void()>& handler)
    {
        this->handler = handler;

        lowPriorityInterrupt.Register([this]()
            {
                progress.Signal();
                if (this->handler)
                    this->handler();
            });
    }

    void SupervisedLowPriorityInterrupt::Unregister()
    {
        handler = nullptr;
        lowPriorityInterrupt.Unregister();
    }
}
