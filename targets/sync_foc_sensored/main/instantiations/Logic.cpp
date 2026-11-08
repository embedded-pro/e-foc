#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "generated/Version.h"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <algorithm>

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
        drivers::EncoderIndex& IndexChannelOr(PlatformFactory& hardware, drivers::EncoderIndex& none)
        {
            const auto channel = hardware.EncoderIndexChannel();
            return channel ? channel->get() : none;
        }

        can::FocFirmwareIdentity FirmwareIdentityOf(BoardKind boardKind)
        {
            return can::FocFirmwareIdentity{ e_foc::generated::VERSION_MAJOR, e_foc::generated::VERSION_MINOR, e_foc::generated::VERSION_PATCH, static_cast<uint8_t>(boardKind) };
        }
    }

    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , boardId{ hardware.BoardId() }
        , ledStatus{ controlMode }
        , statusLeds{ hardware.OperationalLed(), hardware.WarningLed(), hardware.FailureLed(), ledStatus }
        , vdc{ hardware.PowerSupplyVoltage() }
        , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
        , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
        , learnedModelRegion{ hardware.Eeprom(), learnedModelRegionOffset, learnedModelRegionSize }
        , faultHistoryRegion{ hardware.Eeprom(), faultHistoryRegionOffset, faultHistoryRegionSize }
        , nvm{ calibrationRegion, configRegion, faultHistoryRegion, learnedModelRegion }
        , inverter{ hardware }
        , inverterCompensation{ inverter, hardware.InverterDeadTimeLoss(std::chrono::nanoseconds{ pwmDeadTimeNs }) }
        , currentOffsets{ inverter, inverterCompensation }
        , confirmedCurrentOffsets{ *currentOffsets }
        , electricalIdent{ inverterCompensation, hardware, [this]()
            {
                return this->hardware.PowerSupplyVoltage();
            } }
        , motorAlignment{ inverterCompensation, hardware }
        , indexSearch{ inverterCompensation, hardware, IndexChannelOr(hardware, noEncoderIndex) }
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
        , positionLimit{ [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , commandTimeout{ [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , motorOverload{ hardware.MaxCurrentSupported(), [this](foc::Ampere limit)
            {
                overloadLimit = limit;
                ApplyCurrentLimit();
            } }
        , overTemperature{ hardware.MaxCurrentSupported(), [this](foc::Ampere limit)
            {
                thermalLimit = limit;
                ApplyCurrentLimit();
            },
            [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , overloadLimit{ hardware.MaxCurrentSupported() }
        , thermalLimit{ hardware.MaxCurrentSupported() }
        , busVoltageLimits{ [this](state_machine::FaultCode code)
            {
                platformFaultNotifier->Raise(code);
            } }
        , busVoltage{ [this]()
            {
                return this->hardware.PowerSupplyVoltage();
            },
            [this](foc::Volts measured)
            {
                filteredBusVoltage = measured;
                controlMode->TrackBusVoltage(measured);
                busVoltageLimits.Evaluate(measured);
            } }
    {
        services::TraceStartupBanner(hardware.Tracer(), services::StartupBanner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus(), e_foc::generated::VERSION, e_foc::generated::VERSION_GIT_SHA });
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shortest);

        nvm.LoadConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    configData = services::ConfigData{};
                this->hardware.SetEncoderResolution(this->configData.encoderResolution);
                this->hardware.ConfigureCanBus(this->configData.canBaudrate, false);

                const auto nodeId = services::MakeCanNodeId(this->configData.canNodeId, boardId);
                this->hardware.Tracer().Trace() << "[CAN] node " << nodeId << " = base " << this->configData.canNodeId << " + board ID " << boardId;

                tracingCan.emplace(this->hardware.CanBus(), this->hardware.Tracer());
                canBus.emplace(*tracingCan, this->hardware.CanBus(), [](hal::Can::Id id)
                    {
                        return can::FocMotorCurrentStreamer::IsStreamFrame(id);
                    });
                canServer.emplace(*canBus, services::CanProtocolServer::Config{ .nodeId = nodeId });
                tracingServerObserver.emplace(*canServer, this->hardware.Tracer());
                motorCanServer.emplace(canServer->Transport());
                canServer->RegisterCategory(*motorCanServer);
                firmwareUpgradeServer.emplace(canServer->Transport());
                canServer->RegisterCategory(*firmwareUpgradeServer);

                controlMode.emplace(
                    this->hardware.Tracer(),
                    MotorHardware{ inverterCompensation, this->hardware, vdc, this->hardware.DutyUpdateDelayPeriods(), &currentStream },
                    nvm,
                    CalibrationServices{ .electricalIdent = *electricalIdent, .motorAlignment = *motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = confirmedCurrentOffsets, .mapCogging = true, .standstill = rotorStandstill, .identifyFluxLinkage = true, .indexSearch = *indexSearch },
                    *platformFaultNotifier,
                    configData,
                    ControlMode::OuterLoopArgs{
                        .maxCurrent = this->hardware.MaxCurrentSupported(),
                        .baseFrequency = this->hardware.BaseFrequency(),
                        .lowPriorityInterrupt = this->hardware.LowPriorityInterrupt() });
                controlMode->RestoreStoredModel();
                faultHistory.emplace(nvm, [this]()
                    {
                        controlMode->HoldAsyncWork();
                    },
                    [this]()
                    {
                        controlMode->ReleaseAsyncWork();
                    });
                peakCurrent.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                controlMode->OnFaultEntered([this](state_machine::FaultCode code)
                    {
                        RecordFault(code);
                    });
                faultHistory->Start();
                firmwareUpgradeInterlock.emplace(*controlMode);
                firmwareUpgrade.emplace(*firmwareUpgradeServer, this->hardware.Staging(), *firmwareUpgradeInterlock, [this]()
                    {
                        this->hardware.Reset();
                    });
                canBridge.emplace(*motorCanServer, *controlMode,
                    can::FocMotorCanBridge::Platform{ inverter, this->hardware, this->hardware.Diagnostics(), ToCanResetCause(this->hardware.GetResetCause()), FirmwareIdentityOf(this->hardware.GetBoardKind()) },
                    can::FocMotorCanBridge::Services{ *electricalIdent, confirmedCurrentOffsets, rotorStandstill, *firmwareUpgrade, *faultHistory },
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
                overTemperature.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    },
                    [this]()
                    {
                        return controlMode->WindingTemperature();
                    });
                overTemperature.Enable([this]()
                    {
                        return supervision::OverTemperatureSettingsFrom(controlMode->Config());
                    });
                busVoltageLimits.Attach([this]() -> const state_machine::FocStateMachineBase&
                    {
                        return controlMode->ActiveStateMachine();
                    });
                busVoltageLimits.Enable([this]()
                    {
                        return supervision::BusVoltageLimitSettingsFrom(controlMode->Config());
                    });
                busVoltage.Start();
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
                targetReached.Attach([this]() -> const state_machine::FocStateMachineBase*
                    {
                        return controlMode->Active() == state_machine::ControlMode::position ? &controlMode->ActiveStateMachine() : nullptr;
                    });
                targetReached.Enable([this]()
                    {
                        return supervision::TargetReachedSettingsFrom(controlMode->Config());
                    });
                positionLimit.Attach([this]() -> const state_machine::FocStateMachineBase*
                    {
                        return controlMode->Active() == state_machine::ControlMode::position ? &controlMode->ActiveStateMachine() : nullptr;
                    },
                    [this]()
                    {
                        return static_cast<drivers::Encoder&>(this->hardware).Read();
                    });
                positionLimit.Enable([this]()
                    {
                        return supervision::PositionLimitSettingsFrom(controlMode->Config());
                    });
                canBridge->ReportTargetReached([this]()
                    {
                        return targetReached.IsReached();
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

    void Logic::RecordFault(state_machine::FaultCode code)
    {
        faultHistory->Record(static_cast<uint8_t>(can::FocMotorCanBridge::ToCanFaultCode(code)),
            services::FaultHistory::Conditions{ static_cast<uint8_t>(state_machine::ToCanMode(controlMode->Active())), filteredBusVoltage, peakCurrent.Peak() });
    }

    void Logic::ApplyCurrentLimit()
    {
        const auto limit = foc::Ampere{ std::min(overloadLimit.Value(), thermalLimit.Value()) };
        hardware.Tracer().Trace() << "[PRT] Current limit " << limit.Value() << " A";
        controlMode->LimitCurrent(limit);
    }
}
