#pragma once

#include "core/foc/interfaces/Execution.hpp"

namespace supervision
{
    class SupervisedLowPriorityInterrupt
        : public foc::LowPriorityInterrupt
    {
    public:
        SupervisedLowPriorityInterrupt(foc::LowPriorityInterrupt& lowPriorityInterrupt, foc::ProgressSignal& progress);

        // Implementation of foc::LowPriorityInterrupt
        void Trigger() override;
        void Register(const infra::Function<void()>& handler) override;
        void Unregister() override;

    private:
        foc::LowPriorityInterrupt& lowPriorityInterrupt;
        foc::ProgressSignal& progress;
        infra::Function<void()> handler;
    };
}
