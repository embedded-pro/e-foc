#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
        , nvm{ calibrationRegion, configRegion }
        , electricalIdent{ hardware, hardware, vdc }
        , motorAlignment{ hardware, hardware }
    {
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shorter);
        nvm.LoadConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    configData = services::MakeDefaultConfigData();
                this->hardware.SetEncoderResolution(this->configData.encoderResolution);
                this->hardware.ConfigureCanBus(this->configData.canBaudrate, false);
                canTransport.emplace(this->hardware.CanBus(), static_cast<uint16_t>(this->configData.canNodeId));
                motorCanServer.emplace(*canTransport);
                controlMode.emplace(
                    TerminalAndTracer{ terminalWithStorage, this->hardware.Tracer() },
                    MotorHardware{ this->hardware, this->hardware, vdc },
                    nvm,
                    CalibrationServices{ electricalIdent, motorAlignment },
                    noOpFaultNotifier,
                    configData,
                    ControlMode::OuterLoopArgs{
                        this->hardware.MaxCurrentSupported(),
                        this->hardware.BaseFrequency(),
                        this->hardware.LowPriorityInterrupt() });
                canBridge.emplace(*motorCanServer, *controlMode);
            });
    }
}
