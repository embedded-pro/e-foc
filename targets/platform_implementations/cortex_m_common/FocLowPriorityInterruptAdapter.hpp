#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "hal/cortex_m/LowPriorityInterrupt.hpp"

namespace application
{
    class FocLowPriorityInterruptAdapter
        : public foc::LowPriorityInterrupt
    {
    public:
        void Trigger() override { emilLpi.Trigger(); }
        void Register(const infra::Function<void()>& handler) override { emilLpi.Register(handler); }
        void Unregister() override { emilLpi.Unregister(); }

    private:
        hal::cortex::LowPriorityInterrupt emilLpi;
    };
}
