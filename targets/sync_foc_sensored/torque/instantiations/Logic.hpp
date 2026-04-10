#pragma once

#include "foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "source/foc/implementations/FocTorqueImpl.hpp"
#include "source/platform_abstraction/HardwareFactory.hpp"
#include "source/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include "source/services/cli/TerminalTorque.hpp"
#include "source/services/cli/TerminalWithBanner.hpp"
#include "targets/sync_foc_sensored/instantiations/MotorStateMachine.hpp"

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
            foc::FocTorqueImpl,
            services::TerminalFocTorqueInteractor>
            motorStateMachine;
    };
}
