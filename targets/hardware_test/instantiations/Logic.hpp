#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "targets/hardware_test/components/Terminal.hpp"

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::PlatformFactory& hardware);

    private:
        services::TerminalWithBanner::WithMaxSize<20> terminalWithStorage;
        application::TerminalInteractor terminal;
        services::DebugLed debugLed;
    };
}
