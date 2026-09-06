#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalDiagnostics
    {
    public:
        TerminalDiagnostics(services::TerminalWithStorage& terminal, application::PlatformFactory& platform);

    private:
        void PrintControlLoopStatistics();
        void PrintCanStatistics();

        services::TerminalWithStorage& terminal;
        application::PlatformFactory& platform;
    };
}
