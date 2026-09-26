#include "targets/platform_implementations/qemu/implementation/SilWatchdogStall.hpp"

namespace application
{
    SilWatchdogStall::SilWatchdogStall(services::TerminalWithCommands& terminal, services::Tracer& tracer)
        : services::TerminalCommands(terminal)
        , tracer{ tracer }
        , commands{ { Command{ { "watchdog_stall", "wds", "Stall the event loop so the watchdog resets the target. Ex: watchdog_stall" }, [this](const infra::BoundedConstString&)
              {
                  Stall();
              } } } }
    {}

    infra::MemoryRange<const SilWatchdogStall::Command> SilWatchdogStall::Commands()
    {
        return infra::MakeRange(commands);
    }

    void SilWatchdogStall::Stall()
    {
        tracer.Trace() << "[WDT] stalling event loop";
        stallTimer.Start(stallDelay, []()
            {
                volatile bool stalled{ true };
                while (stalled)
                {
                    // Never returns, so the dispatcher stops making progress
                }
            });
    }
}
