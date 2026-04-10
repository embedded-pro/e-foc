#pragma once

#include "foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "targets/sync_foc_sensored/instantiations/MotorStateMachine.hpp"

namespace application
{
    template<typename FocImpl, typename TerminalImpl>
    class LogicWithOuterLoop
    {
    public:
        explicit LogicWithOuterLoop(application::PlatformFactory& hardware, infra::BoundedConstString bannerName)
            : platformAdapter{ hardware }
            , debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
            , vdc{ hardware.PowerSupplyVoltage() }
            , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ bannerName, vdc, hardware.SystemClock() } }
            , motorStateMachine(
                  TerminalAndTracer{ terminalWithStorage, hardware.Tracer() },
                  MotorDriverAndEncoder{ platformAdapter, platformAdapter },
                  vdc, hardware.Eeprom(), hardware.MaxCurrentSupported(), platformAdapter.BaseFrequency(), hardware.LowPriorityInterrupt())
        {}

    private:
        PlatformAdapter platformAdapter;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<10> terminalWithStorage;
        MotorStateMachine<FocImpl, TerminalImpl> motorStateMachine;
    };
}
