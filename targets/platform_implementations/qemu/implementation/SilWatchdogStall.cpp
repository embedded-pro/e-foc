#include "targets/platform_implementations/qemu/implementation/SilWatchdogStall.hpp"

namespace application
{
    SilWatchdogStall::SilWatchdogStall(services::Tracer& tracer)
        : tracer{ tracer }
    {}

    void SilWatchdogStall::Schedule(infra::Duration delay)
    {
        tracer.Trace() << "[WDT] event loop stalls in " << std::chrono::duration_cast<std::chrono::milliseconds>(delay).count() << " ms";
        stallTimer.Start(delay, []()
            {
                volatile bool stalled{ true };
                while (stalled)
                {
                    // Never returns, so the dispatcher stops making progress
                }
            });
    }
}
