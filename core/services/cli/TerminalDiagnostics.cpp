#include "core/services/cli/TerminalDiagnostics.hpp"
#include <array>

namespace services
{
    namespace
    {
        constexpr std::array<application::CanBusAdapter::CanError, application::CanBusAdapter::errorClasses> allErrors{
            application::CanBusAdapter::CanError::busOff,
            application::CanBusAdapter::CanError::errorPassive,
            application::CanBusAdapter::CanError::errorWarning,
            application::CanBusAdapter::CanError::messageLost,
            application::CanBusAdapter::CanError::rxBufferOverflow,
            application::CanBusAdapter::CanError::ackError,
            application::CanBusAdapter::CanError::stuffError,
            application::CanBusAdapter::CanError::formError,
            application::CanBusAdapter::CanError::crcError,
            application::CanBusAdapter::CanError::bit0Error,
            application::CanBusAdapter::CanError::bit1Error,
            application::CanBusAdapter::CanError::other
        };
    }

    TerminalDiagnostics::TerminalDiagnostics(services::TerminalWithStorage& terminal, application::PlatformFactory& platform)
        : terminal(terminal)
        , platform(platform)
    {
        terminal.AddCommand({ { "loop_stats", "ls", "Print control loop execution statistics" },
            [this](const infra::BoundedConstString&)
            {
                PrintControlLoopStatistics();
            } });

        terminal.AddCommand({ { "can_stats", "cs", "Print CAN error counters by class" },
            [this](const infra::BoundedConstString&)
            {
                PrintCanStatistics();
            } });

        terminal.AddCommand({ { "clear_stats", "xs", "Reset control loop and CAN statistics" },
            [this](const infra::BoundedConstString&)
            {
                this->platform.ResetStatistics();
            } });
    }

    void TerminalDiagnostics::PrintControlLoopStatistics()
    {
        const auto statistics = platform.ControlLoopStatistics();
        auto& tracer = platform.Tracer();

        tracer.Trace() << "[LOOP] samples=" << statistics.samples
                       << " budget=" << statistics.budgetCycles << " cycles";
        tracer.Trace() << "[LOOP] cycles last=" << statistics.lastCycles
                       << " min=" << statistics.minimumCycles
                       << " avg=" << statistics.averageCycles
                       << " max=" << statistics.maximumCycles;
        tracer.Trace() << "[LOOP] overruns=" << statistics.overruns
                       << " deadlineMisses=" << statistics.deadlineMisses
                       << " reentries=" << statistics.reentries;
    }

    void TerminalDiagnostics::PrintCanStatistics()
    {
        const auto& counters = platform.CanStatistics();
        auto& tracer = platform.Tracer();

        tracer.Trace() << "[CAN] total errors=" << counters.Total();

        for (const auto error : allErrors)
        {
            const auto count = counters.Count(error);

            if (count != 0)
                tracer.Trace() << "[CAN] " << error << "=" << count;
        }
    }
}
