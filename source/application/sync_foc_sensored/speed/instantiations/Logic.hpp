#pragma once

#include "foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "services/util/DebugLed.hpp"
#include "source/application/sync_foc_sensored/instantiations/MotorStateMachine.hpp"
#include "source/foc/implementations/SpeedControllerImpl.hpp"
#include "source/foc/instantiations/FieldOrientedControllerImpl.hpp"
#include "source/hardware/HardwareFactory.hpp"
#include "source/hardware/MotorFieldOrientedControllerAdapter.hpp"
#include "source/services/cli/TerminalSpeed.hpp"
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
            foc::FieldOrientedControllerSpeedImpl,
            foc::WithAutomaticCurrentPidGains<foc::SpeedControllerImpl>,
            services::TerminalFocSpeedInteractor>
            motorStateMachine;
    };
}
