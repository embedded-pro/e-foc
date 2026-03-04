#pragma once

#include "foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "services/util/DebugLed.hpp"
#include "source/application/sync_foc_sensored/instantiations/MotorStateMachine.hpp"
#include "source/foc/implementations/FocPositionImpl.hpp"
#include "source/hardware/HardwareFactory.hpp"
#include "source/hardware/MotorFieldOrientedControllerAdapter.hpp"
#include "source/services/cli/TerminalPosition.hpp"
#include "source/services/cli/TerminalWithBanner.hpp"

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::HardwareFactory& hardware);

    private:
        HardwareAdapter hardwareAdapter;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<10> terminalWithStorage;
        MotorStateMachine<
            foc::FocPositionImpl,
            services::TerminalFocPositionInteractor>
            motorStateMachine;
    };
}
