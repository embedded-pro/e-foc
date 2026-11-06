#include "core/services/cli/TerminalWithBanner.hpp"

namespace services
{
    TerminalWithBanner::TerminalWithBanner(infra::BoundedVector<Command>& storage, services::TerminalWithCommands& terminal, services::Tracer& tracer, const Banner& banner)
        : TerminalWithStorage(storage, terminal, tracer)
    {
        TraceStartupBanner(tracer, banner);
        tracer.Trace() << "Ready to accept commands. Type 'help' for available commands.";
    }
}
