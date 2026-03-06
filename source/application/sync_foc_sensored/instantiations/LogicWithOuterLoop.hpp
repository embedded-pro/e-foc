#pragma once

#include "foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "services/util/DebugLed.hpp"
#include "source/application/sync_foc_sensored/instantiations/MotorStateMachine.hpp"
#include "source/hardware/HardwareFactory.hpp"
#include "source/hardware/MotorFieldOrientedControllerAdapter.hpp"
#include "source/services/cli/TerminalWithBanner.hpp"

namespace application
{
    template<typename FocImpl, typename TerminalImpl>
    class LogicWithOuterLoop
    {
    public:
        explicit LogicWithOuterLoop(application::HardwareFactory& hardware, infra::BoundedConstString bannerName)
            : hardwareAdapter{ hardware }
            , debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
            , vdc{ hardware.PowerSupplyVoltage() }
            , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ bannerName, vdc, hardware.SystemClock() } }
            , motorStateMachine(
                  TerminalAndTracer{ terminalWithStorage, hardware.Tracer() },
                  MotorDriverAndEncoder{ hardwareAdapter, hardwareAdapter },
                  vdc, hardware.MaxCurrentSupported(), hardwareAdapter.BaseFrequency(), hardware.LowPriorityInterrupt())
        {}

    private:
        HardwareAdapter hardwareAdapter;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<10> terminalWithStorage;
        MotorStateMachine<FocImpl, TerminalImpl> motorStateMachine;
    };
}
