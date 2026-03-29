#pragma once

#include "services/peripheral/DebugLed.hpp"
#include "source/application/hardware_test/components/Terminal.hpp"
#include "source/hardware/HardwareFactory.hpp"
#include "source/services/cli/TerminalWithBanner.hpp"

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::HardwareFactory& hardware);

    private:
        services::TerminalWithBanner::WithMaxSize<10> terminalWithStorage;
        application::TerminalInteractor terminal;
        services::DebugLed debugLed;
    };
}
