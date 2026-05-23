#include "targets/sync_foc_sensored/torque/instantiations/Logic.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored:torque", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , calibrationRegion{ hardware.Eeprom(), 0, 128 }
        , configRegion{ hardware.Eeprom(), 128, 128 }
        , nvm{ calibrationRegion, configRegion }
        , electricalIdent{ hardware, hardware, vdc }
        , motorAlignment{ hardware, hardware }
        , motorStateMachine(
              TerminalAndTracer{ terminalWithStorage, hardware.Tracer() },
              MotorHardware{ hardware, hardware, vdc },
              nvm,
              CalibrationServices{ electricalIdent, motorAlignment },
              noOpFaultNotifier)
    {
        hardware.ConfigureAdcAndPwm(hal::Hertz{ 10000 }, std::chrono::nanoseconds{ 500 }, PlatformFactory::SampleAndHold::shorter);
        hardware.SetEncoderResolution(4000);
        hardware.ConfigureCanBus(1'000'000, false);
    }
}
