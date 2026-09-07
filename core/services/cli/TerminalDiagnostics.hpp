#pragma once

#include "core/platform_abstraction/PlatformDiagnostics.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalDiagnostics
    {
    public:
        TerminalDiagnostics(services::TerminalWithStorage& terminal, application::PlatformDiagnostics& diagnostics, services::Tracer& tracer);

    private:
        void PrintControlLoopStatistics();
        void PrintCanStatistics();

        application::PlatformDiagnostics& diagnostics;
        services::Tracer& tracer;
    };
}
