#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "core/foc/interfaces/IsrProfile.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , terminalDiagnostics{ terminalWithStorage, hardware.Diagnostics(), hardware.Tracer() }
        , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
        , nvm{ calibrationRegion, configRegion }
        , currentOffsets{ hardware }
        , deadTimeCompensation{ *currentOffsets, hardware.InverterDeadTimeLoss(std::chrono::nanoseconds{ pwmDeadTimeNs }) }
        , electricalIdent{ deadTimeCompensation, hardware, vdc }
        , motorAlignment{ deadTimeCompensation, hardware }
        , platformFaultNotifier{ hardware }
        , encoderPlausibility{ hardware, hardware.MaxCurrentSupported(), [this]()
            {
                platformFaultNotifier->Raise(state_machine::FaultCode::encoderLoss);
            } }
    {
        terminalWithStorage.AddCommand({ { "isr_profile", "ip", "Print the control interrupt's per-stage cycles" },
            [this](const infra::BoundedConstString&)
            {
                for (std::size_t stage = 0; stage < diag::IsrProfile::stages; ++stage)
                    this->hardware.Tracer().Trace() << "[ISRP] " << diag::isrStageNames[stage]
                                                    << " hits=" << diag::isrProfile.Hits(stage)
                                                    << " avg=" << diag::isrProfile.Average(stage)
                                                    << " max=" << diag::isrProfile.Maximum(stage);
            } });

        terminalWithStorage.AddCommand({ { "isr_profile_clear", "ipc", "Reset the control interrupt's per-stage cycles" },
            [](const infra::BoundedConstString&)
            {
                diag::isrProfile.Clear();
            } });

        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shortest);

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
                    MotorHardware{ deadTimeCompensation, this->hardware, vdc },
                    nvm,
                    CalibrationServices{ .electricalIdent = *electricalIdent, .motorAlignment = *motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = *currentOffsets, .mapCogging = true },
                    *platformFaultNotifier,
                    configData,
                    ControlMode::OuterLoopArgs{
                        .maxCurrent = this->hardware.MaxCurrentSupported(),
                        .baseFrequency = this->hardware.BaseFrequency(),
                        .lowPriorityInterrupt = this->hardware.LowPriorityInterrupt(),
                        .dutyUpdateDelayPeriods = this->hardware.DutyUpdateDelayPeriods() });
                canBridge.emplace(*motorCanServer, *controlMode, this->hardware, this->hardware, *electricalIdent, nullptr, nvm, configData, this->hardware.Tracer());
                canLivenessWatchdog.emplace(*canServer, *controlMode, this->hardware.Tracer());
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
