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
                controlMode->Commands().TrackBusVoltage(measured);
                busVoltageLimits.Evaluate(measured);
            } }
    {
        services::TraceStartupBanner(hardware.Tracer(), services::StartupBanner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus(), e_foc::generated::VERSION, e_foc::generated::VERSION_GIT_SHA });
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shortest);

        nvm.LoadConfig(configData, [this](services::NvmStatus status)
            {
                OnConfigLoaded(status);
            });
    }

    void Logic::OnConfigLoaded(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
            configData = services::ConfigData{};
        hardware.SetEncoderResolution(configData.encoderResolution);
        hardware.ConfigureCanBus(configData.canBaudrate, false);

        StartCan();
        StartControl();
        StartFirmwareUpgradeAndBridge();
        SuperviseDrive();
        currentStreamer.emplace(*canServer, *motorCanServer, currentStream, controlMode->Config(), inverterCompensation.BaseFrequency());
        SuperviseEncoder();
        SuperviseMotion();
        BroadcastFaults();
    }

    void Logic::StartCan()
    {
        const auto nodeId = services::MakeCanNodeId(configData.canNodeId, boardId);
        hardware.Tracer().Trace() << "[CAN] node " << nodeId << " = base " << configData.canNodeId << " + board ID " << boardId;

        tracingCan.emplace(hardware.CanBus(), hardware.Tracer());
        canBus.emplace(*tracingCan, hardware.CanBus(), [](hal::Can::Id id)
            {
                return can::FocMotorCurrentStreamer::IsStreamFrame(id);
            });
        canServer.emplace(*canBus, services::CanProtocolServer::Config{ .nodeId = nodeId });
        tracingServerObserver.emplace(*canServer, hardware.Tracer());
        motorCanServer.emplace(canServer->Transport());
        canServer->RegisterCategory(*motorCanServer);
        firmwareUpgradeServer.emplace(canServer->Transport());
        canServer->RegisterCategory(*firmwareUpgradeServer);
    }

    void Logic::StartControl()
    {
        controlMode.emplace(
            hardware.Tracer(),
            MotorHardware{ inverterCompensation, hardware, vdc, hardware.DutyUpdateDelayPeriods(), &currentStream },
            nvm,
            CalibrationServices{ .electricalIdent = *electricalIdent, .motorAlignment = *motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = confirmedCurrentOffsets, .mapCogging = true, .standstill = rotorStandstill, .identifyFluxLinkage = true, .indexSearch = *indexSearch },
            *platformFaultNotifier,
            configData,
            ControlMode::OuterLoopArgs{
                .maxCurrent = hardware.MaxCurrentSupported(),
                .baseFrequency = hardware.BaseFrequency(),
                .lowPriorityInterrupt = hardware.LowPriorityInterrupt() });
        controlMode->Recalibration().RestoreStoredModel();
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
                return controlMode->Drive().ActiveStateMachine();
            });
        controlMode->OnFaultEntered([this](state_machine::FaultCode code)
            {
                RecordFault(code);
            });
        faultHistory->Start();
    }

    void Logic::StartFirmwareUpgradeAndBridge()
    {
        firmwareUpgradeInterlock.emplace(*controlMode);
        firmwareUpgrade.emplace(*firmwareUpgradeServer, hardware.Staging(), *firmwareUpgradeInterlock, [this]()
            {
                hardware.Reset();
            });
        canBridge.emplace(*motorCanServer, *controlMode,
            can::FocMotorCanBridge::Platform{ inverter, hardware, hardware.Diagnostics(), ToCanResetCause(hardware.GetResetCause()), FirmwareIdentityOf(hardware.GetBoardKind()) },
            can::FocMotorCanBridge::Services{ *electricalIdent, confirmedCurrentOffsets, rotorStandstill, *firmwareUpgrade, *faultHistory },
            hardware.Tracer());
        canLivenessWatchdog.emplace(*canServer, *controlMode, hardware.Tracer());
        canBridge->ObserveCommands([this]()
            {
                commandTimeout.CommandAccepted();
            });
    }

    void Logic::SuperviseDrive()
    {
        commandTimeout.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        motorOverload.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        motorOverload.Enable([this]()
            {
                return supervision::OverloadRatingsFrom(controlMode->Config());
            });
        overTemperature.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            },
            [this]()
            {
                return controlMode->Drive().WindingTemperature();
            });
        overTemperature.Enable([this]()
            {
                return supervision::OverTemperatureSettingsFrom(controlMode->Config());
            });
        busVoltageLimits.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
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
    }

    void Logic::SuperviseEncoder()
    {
        encoderPlausibility.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        encoderPlausibility.Enable([this]()
            {
                return supervision::ConfigFrom(controlMode->Config());
            });
        encoderFreeze.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        encoderFreeze.Enable([this]()
            {
                return supervision::LimitsFrom(controlMode->Drive().CalibrationRecord(), controlMode->Config().encoderResolution, hardware.MaxCurrentSupported());
            });
        phaseLoss.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        phaseLoss.Enable([this]()
            {
                return foc::RadiansPerSecond{ controlMode->Config().currentLoop.bandwidth };
            });
    }

    void Logic::SuperviseMotion()
    {
        overspeed.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        overspeed.Enable([this]()
            {
                return foc::RadiansPerSecond{ controlMode->Config().limits.overspeedLimit };
            });
        followingError.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return controlMode->Drive().ActiveStateMachine();
            });
        followingError.Enable([this]()
            {
                return supervision::FollowingErrorSettingsFrom(controlMode->Config());
            });
        targetReached.Attach([this]() -> const state_machine::FocStateMachineBase*
            {
                return controlMode->Drive().Active() == state_machine::ControlMode::position ? &controlMode->Drive().ActiveStateMachine() : nullptr;
            });
        targetReached.Enable([this]()
            {
                return supervision::TargetReachedSettingsFrom(controlMode->Config());
            });
        positionLimit.Attach([this]() -> const state_machine::FocStateMachineBase*
            {
                return controlMode->Drive().Active() == state_machine::ControlMode::position ? &controlMode->Drive().ActiveStateMachine() : nullptr;
            },
            [this]()
            {
                return static_cast<drivers::Encoder&>(hardware).Read();
            });
        positionLimit.Enable([this]()
            {
                return supervision::PositionLimitSettingsFrom(controlMode->Config());
            });
        canBridge->ReportTargetReached([this]()
            {
                return targetReached.IsReached();
            });
    }

    void Logic::BroadcastFaults()
    {
        platformFaultNotifier->RegisterSecondary([this](state_machine::FaultCode code)
            {
                infra::EventDispatcherWithWeakPtr::Instance().Schedule([this, code]()
                    {
                        canBridge->BroadcastFault(code);
                    });
            });
    }

    void Logic::RecordFault(state_machine::FaultCode code)
    {
        faultHistory->Record(static_cast<uint8_t>(can::FocMotorCanBridge::ToCanFaultCode(code)),
            services::FaultHistory::Conditions{ static_cast<uint8_t>(state_machine::ToCanMode(controlMode->Drive().Active())), filteredBusVoltage, peakCurrent.Peak() });
    }

    void Logic::ApplyCurrentLimit()
    {
        const auto limit = foc::Ampere{ std::min(overloadLimit.Value(), thermalLimit.Value()) };
        hardware.Tracer().Trace() << "[PRT] Current limit " << limit.Value() << " A";
        controlMode->Commands().LimitCurrent(limit);
    }
}
