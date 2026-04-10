#pragma once

#include "foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include "core/services/cli/TerminalTorque.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "targets/sync_foc_sensored/instantiations/MotorStateMachine.hpp"

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::PlatformFactory& hardware);

    private:
        PlatformAdapter platformAdapter;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<10> terminalWithStorage;
        MotorStateMachine<
            foc::FocTorqueImpl,
            services::TerminalFocTorqueInteractor>
            motorStateMachine;
    };
}
