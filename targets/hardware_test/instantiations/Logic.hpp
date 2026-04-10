#pragma once

#include "services/peripheral/DebugLed.hpp"
#include "targets/hardware_test/components/Terminal.hpp"
#include "source/platform_abstraction/HardwareFactory.hpp"
#include "source/services/cli/TerminalWithBanner.hpp"

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::HardwareFactory& hardware);

    private:
        services::TerminalWithBanner::WithMaxSize<15> terminalWithStorage;
        application::TerminalInteractor terminal;
        services::DebugLed debugLed;
    };
}
