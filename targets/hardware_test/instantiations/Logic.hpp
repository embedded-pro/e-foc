#pragma once

#include "services/peripheral/DebugLed.hpp"
#include "targets/hardware_test/components/Terminal.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::PlatformFactory& hardware);

    private:
        services::TerminalWithBanner::WithMaxSize<15> terminalWithStorage;
        application::TerminalInteractor terminal;
        services::DebugLed debugLed;
    };
}
