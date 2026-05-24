#include "targets/sync_foc_sensored/torque/instantiations/Logic.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored:torque", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
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
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shorter);
        nvm.LoadConfig(configData, [this](services::NvmStatus)
            {
                this->hardware.SetEncoderResolution(this->configData.encoderResolution);
                this->hardware.ConfigureCanBus(this->configData.canBaudrate, false);
            });
    }
}
