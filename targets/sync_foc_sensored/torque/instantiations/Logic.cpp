#include "targets/sync_foc_sensored/torque/instantiations/Logic.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : platformAdapter{ hardware }
        , debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored:torque", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , calibrationRegion{ hardware.Eeprom(), 0, 128 }
        , configRegion{ hardware.Eeprom(), 128, 128 }
        , nvm{ calibrationRegion, configRegion }
        , electricalIdent{ platformAdapter, platformAdapter, vdc }
        , motorAlignment{ platformAdapter, platformAdapter }
        , motorStateMachine(
              TerminalAndTracer{ terminalWithStorage, hardware.Tracer() },
              MotorHardware{ platformAdapter, platformAdapter, vdc },
              nvm,
              CalibrationServices{ electricalIdent, motorAlignment },
              noOpFaultNotifier)
    {}
}
