#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace application
{
    Logic::WatchdogSupervision::WatchdogSupervision(application::PlatformFactory& hardware, services::Tracer& tracer)
        : inverter{ hardware, health.InnerLoopProgress() }
        , lowPriorityInterrupt{ hardware.LowPriorityInterrupt(), health.OuterLoopProgress() }
        , supervisor{ hardware, health, tracer }
    {}

    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , terminalDiagnostics{ terminalWithStorage, hardware.Diagnostics(), hardware.Tracer() }
        , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
        , nvm{ calibrationRegion, configRegion }
        , electricalIdent{ hardware, hardware, vdc }
        , motorAlignment{ hardware, hardware }
        , platformFaultNotifier{ hardware }
        , watchdog{ hardware, hardware.Tracer() }
        , encoderPlausibility{ hardware, hardware.MaxCurrentSupported(), [this]()
            {
                platformFaultNotifier->Raise(state_machine::FaultCode::encoderLoss);
            } }
    {
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shorter);

        watchdog.supervisor.Enable({ .deadline = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds(watchdogDeadlineMs)),
            .startupGrace = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds(watchdogStartupGraceMs)),
            .evaluationsPerDeadline = watchdogEvaluationsPerDeadline });

        nvm.LoadConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    configData = services::MakeDefaultConfigData();
                this->hardware.SetEncoderResolution(this->configData.encoderResolution);
                this->hardware.ConfigureCanBus(this->configData.canBaudrate, false);

                tracingCan.emplace(this->hardware.CanBus(), this->hardware.Tracer());
                canServer.emplace(*tracingCan, services::CanProtocolServer::Config{
                                                   .nodeId = static_cast<uint16_t>(this->configData.canNodeId) });
                tracingServerObserver.emplace(*canServer, this->hardware.Tracer());
                motorCanServer.emplace(canServer->Transport());
                canServer->RegisterCategory(*motorCanServer);

                controlMode.emplace(
                    TerminalAndTracer{ terminalWithStorage, this->hardware.Tracer() },
                    MotorHardware{ watchdog.inverter, this->hardware, vdc },
                    nvm,
                    CalibrationServices{ .electricalIdent = electricalIdent, .motorAlignment = motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb } },
                    *platformFaultNotifier,
                    configData,
                    ControlMode::OuterLoopArgs{
                        this->hardware.MaxCurrentSupported(),
                        this->hardware.BaseFrequency(),
                        watchdog.lowPriorityInterrupt });
                canBridge.emplace(*motorCanServer, *controlMode, this->hardware, this->hardware, electricalIdent, nullptr, nvm, configData, this->hardware.Tracer());
                canLivenessWatchdog.emplace(*canServer, *controlMode, this->hardware.Tracer());
                watchdog.supervisor.AttachControlMode(*controlMode);
                encoderPlausibility.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                encoderPlausibility.Enable({});
                platformFaultNotifier->RegisterSecondary([this](state_machine::FaultCode code)
                    {
                        infra::EventDispatcherWithWeakPtr::Instance().Schedule([this, code]()
                            {
                                canBridge->BroadcastFault(code);
                            });
                    });
            });
    }
}
