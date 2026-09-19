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
        // Stored before the trampoline is attached, and cleared only after it is detached, so the
        // interrupt never reaches a handler that is being written
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
        lowPriorityInterrupt.Unregister();
        handler = nullptr;
    }
}
