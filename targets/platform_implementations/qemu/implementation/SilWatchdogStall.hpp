#pragma once

#include "infra/timer/Timer.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    // Lets a SIL scenario stall the event loop, so the watchdog reset path runs in CI on the emulated target
    class SilWatchdogStall
    {
    public:
        explicit SilWatchdogStall(services::Tracer& tracer);

        void Schedule(infra::Duration delay);

    private:
        services::Tracer& tracer;
        infra::TimerSingleShot stallTimer;
    };
}
