#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    // Prints what the control loop and the CAN bus have been doing. #255 reports that every one of
    // these numbers is computed or available somewhere and none of it is reachable from outside.
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
