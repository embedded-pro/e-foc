#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "core/can/CanNodeAddress.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace application
{
    namespace
    {
        can::FocResetCause ToCanResetCause(ResetCause cause)
        {
            switch (cause)
            {
                case ResetCause::brownOut:
                    return can::FocResetCause::brownOut;
                case ResetCause::software:
                    return can::FocResetCause::software;
                case ResetCause::hardware:
                    return can::FocResetCause::hardware;
                case ResetCause::watchdog:
                    return can::FocResetCause::watchdog;
                default:
                    return can::FocResetCause::powerUp;
            }
        }
    }

    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , boardId{ hardware.BoardId() }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
        , terminalDiagnostics{ terminalWithStorage, hardware.Diagnostics(), hardware.Tracer() }
        , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
        , nvm{ calibrationRegion, configRegion }
        , inverter{ hardware }
        , inverterCompensation{ inverter, hardware.InverterDeadTimeLoss(std::chrono::nanoseconds{ pwmDeadTimeNs }) }
        , currentOffsets{ inverter, inverterCompensation }
        , confirmedCurrentOffsets{ *currentOffsets }
        , electricalIdent{ inverterCompensation, hardware, vdc }
        , motorAlignment{ inverterCompensation, hardware }
        , rotorStandstill{ hardware }
        , platformFaultNotifier{ hardware }
        , encoderPlausibility{ hardware, hardware.MaxCurrentSupported(), [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
    {
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shortest);

        nvm.LoadConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    configData = services::MakeDefaultConfigData();
                this->hardware.SetEncoderResolution(this->configData.encoderResolution);
                this->hardware.ConfigureCanBus(this->configData.canBaudrate, false);

                const auto nodeId = can::CanNodeId(this->configData.canNodeId, boardId);
                this->hardware.Tracer().Trace() << "[CAN] node " << nodeId << " = base " << this->configData.canNodeId << " + board ID " << boardId;

                tracingCan.emplace(this->hardware.CanBus(), this->hardware.Tracer());
                canServer.emplace(*tracingCan, services::CanProtocolServer::Config{ .nodeId = nodeId });
                tracingServerObserver.emplace(*canServer, this->hardware.Tracer());
                motorCanServer.emplace(canServer->Transport());
                canServer->RegisterCategory(*motorCanServer);

                controlMode.emplace(
                    TerminalAndTracer{ terminalWithStorage, this->hardware.Tracer() },
                    MotorHardware{ inverterCompensation, this->hardware, vdc, this->hardware.DutyUpdateDelayPeriods() },
                    nvm,
                    CalibrationServices{ .electricalIdent = *electricalIdent, .motorAlignment = *motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = confirmedCurrentOffsets, .mapCogging = true, .standstill = rotorStandstill, .identifyFluxLinkage = true },
                    *platformFaultNotifier,
                    configData,
                    ControlMode::OuterLoopArgs{
                        .maxCurrent = this->hardware.MaxCurrentSupported(),
                        .baseFrequency = this->hardware.BaseFrequency(),
                        .lowPriorityInterrupt = this->hardware.LowPriorityInterrupt() });
                canBridge.emplace(*motorCanServer, *controlMode,
                    can::FocMotorCanBridge::Platform{ inverter, this->hardware, this->hardware.Diagnostics(), ToCanResetCause(this->hardware.GetResetCause()) },
                    *electricalIdent, this->hardware.Tracer(), confirmedCurrentOffsets, rotorStandstill);
                canLivenessWatchdog.emplace(*canServer, *controlMode, this->hardware.Tracer());
                encoderPlausibility.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                encoderPlausibility.Enable([this]()
                    {
                        return supervision::ConfigFrom(controlMode->Config());
                    });
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
