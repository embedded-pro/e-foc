#include "source/application/sync_foc_sensored/torque/instantiations/Logic.hpp"

namespace application
{
    Logic::Logic(application::HardwareFactory& hardware)
        : hardwareAdapter{ hardware }
        , debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored:torque", vdc, hardware.SystemClock() } }
        , motorStateMachine(
              TerminalAndTracer{ terminalWithStorage, hardware.Tracer() },
              MotorDriverAndEncoder{ hardwareAdapter, hardwareAdapter },
              vdc, hardware.Eeprom())
    {}
}
