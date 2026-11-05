#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "core/can/CanNodeAddress.hpp"
#include "generated/Version.h"
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

    namespace
    {
        can::FocFirmwareIdentity FirmwareIdentityOf(BoardKind boardKind)
        {
            return can::FocFirmwareIdentity{ e_foc::generated::VERSION_MAJOR, e_foc::generated::VERSION_MINOR, e_foc::generated::VERSION_PATCH, static_cast<uint8_t>(boardKind) };
        }
    }

    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , boardId{ hardware.BoardId() }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
        , vdc{ hardware.PowerSupplyVoltage() }
        , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus(), e_foc::generated::VERSION, e_foc::generated::VERSION_GIT_SHA } }
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
        , encoderFreeze{ hardware, [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , phaseLoss{ hardware.MaxCurrentSupported(), [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , overspeed{ [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , followingError{ [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , commandTimeout{ [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , motorOverload{ hardware.MaxCurrentSupported(), [this](foc::Ampere limit)
            {
                this->hardware.Tracer().Trace() << "[PRT] Current limit " << limit.Value() << " A";
                controlMode->LimitCurrent(limit);
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
                canBus.emplace(*tracingCan, this->hardware.CanBus());
                canServer.emplace(*canBus, services::CanProtocolServer::Config{ .nodeId = nodeId });
                tracingServerObserver.emplace(*canServer, this->hardware.Tracer());
                motorCanServer.emplace(canServer->Transport());
                canServer->RegisterCategory(*motorCanServer);
                firmwareUpgradeServer.emplace(canServer->Transport());
                canServer->RegisterCategory(*firmwareUpgradeServer);

                controlMode.emplace(
                    TerminalAndTracer{ terminalWithStorage, this->hardware.Tracer() },
                    MotorHardware{ inverterCompensation, this->hardware, vdc, this->hardware.DutyUpdateDelayPeriods(), &currentStream },
                    nvm,
                    CalibrationServices{ .electricalIdent = *electricalIdent, .motorAlignment = *motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = confirmedCurrentOffsets, .mapCogging = true, .standstill = rotorStandstill, .identifyFluxLinkage = true },
                    *platformFaultNotifier,
                    configData,
                    ControlMode::OuterLoopArgs{
                        .maxCurrent = this->hardware.MaxCurrentSupported(),
                        .baseFrequency = this->hardware.BaseFrequency(),
                        .lowPriorityInterrupt = this->hardware.LowPriorityInterrupt() });
                firmwareUpgradeInterlock.emplace(*controlMode);
                firmwareUpgrade.emplace(*firmwareUpgradeServer, this->hardware.Staging(), *firmwareUpgradeInterlock, [this]()
                    {
                        this->hardware.Reset();
                    });
                canBridge.emplace(*motorCanServer, *controlMode,
                    can::FocMotorCanBridge::Platform{ inverter, this->hardware, this->hardware.Diagnostics(), ToCanResetCause(this->hardware.GetResetCause()), FirmwareIdentityOf(this->hardware.GetBoardKind()) },
                    can::FocMotorCanBridge::Services{ *electricalIdent, confirmedCurrentOffsets, rotorStandstill, *firmwareUpgrade },
                    this->hardware.Tracer());
                canLivenessWatchdog.emplace(*canServer, *controlMode, this->hardware.Tracer());
                canBridge->ObserveCommands([this]()
                    {
                        commandTimeout.CommandAccepted();
                    });
                commandTimeout.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                motorOverload.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                motorOverload.Enable([this]()
                    {
                        return supervision::OverloadRatingsFrom(controlMode->Config());
                    });
                commandTimeout.Enable([this]()
                    {
                        return std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(controlMode->Config().limits.commandTimeout));
                    });
                currentStreamer.emplace(*canServer, *motorCanServer, currentStream, controlMode->Config(), inverterCompensation.BaseFrequency());
                encoderPlausibility.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                encoderPlausibility.Enable([this]()
                    {
                        return supervision::ConfigFrom(controlMode->Config());
                    });
                encoderFreeze.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                encoderFreeze.Enable([this]()
                    {
                        return supervision::LimitsFrom(controlMode->CalibrationRecord(), controlMode->Config().encoderResolution, this->hardware.MaxCurrentSupported());
                    });
                phaseLoss.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                phaseLoss.Enable([this]()
                    {
                        return foc::RadiansPerSecond{ controlMode->Config().currentLoop.bandwidth };
                    });
                overspeed.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                overspeed.Enable([this]()
                    {
                        return foc::RadiansPerSecond{ controlMode->Config().limits.overspeedLimit };
                    });
                followingError.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                followingError.Enable([this]()
                    {
                        return supervision::FollowingErrorSettingsFrom(controlMode->Config());
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
