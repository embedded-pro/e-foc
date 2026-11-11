#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/services/cli/StartupBanner.hpp"
#include "generated/Version.h"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : hardware{ hardware }
        , boardId{ hardware.BoardId() }
        , ledStatus{ controlMode }
        , statusLeds{ hardware.OperationalLed(), hardware.WarningLed(), hardware.FailureLed(), ledStatus }
        , vdc{ hardware.PowerSupplyVoltage() }
        , storage{ hardware.Eeprom() }
        , inverter{ hardware }
        , inverterCompensation{ inverter, hardware.InverterDeadTimeLoss(std::chrono::nanoseconds{ pwmDeadTimeNs }) }
        , calibration{ hardware, inverter, inverterCompensation }
        , platformFaultNotifier{ hardware }
        , supervision{ hardware, controlMode, platformFaultNotifier }
        , busVoltage{ [this]()
            {
                return this->hardware.PowerSupplyVoltage();
            },
            [this](foc::Volts measured)
            {
                filteredBusVoltage = measured;
                controlMode->Commands().TrackBusVoltage(measured);
                supervision.EvaluateBusVoltage(measured);
            } }
    {
        services::TraceStartupBanner(hardware.Tracer(), services::StartupBanner{ "sync_foc_sensored", vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus(), e_foc::generated::VERSION, e_foc::generated::VERSION_GIT_SHA });
        hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shortest);

        storage.Nvm().LoadConfig(configData, [this](services::NvmStatus status)
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
        StartCanServices();
        supervision.SuperviseDrive();
        busVoltage.Start();
        supervision.SuperviseEncoder();
        supervision.SuperviseMotion();
        canServices->canBridge.ReportTargetReached([this]()
            {
                return supervision.IsTargetReached();
            });
        BroadcastFaults();
    }

    void Logic::StartCan()
    {
        const auto nodeId = services::MakeCanNodeId(configData.canNodeId, boardId);
        hardware.Tracer().Trace() << "[CAN] node " << nodeId << " = base " << configData.canNodeId << " + board ID " << boardId;
        canStack.emplace(hardware, nodeId);
    }

    void Logic::StartControl()
    {
        controlMode.emplace(
            hardware.Tracer(),
            MotorHardware{ inverterCompensation, hardware, currentStream, vdc, hardware.DutyUpdateDelayPeriods(), &hardware },
            storage.Nvm(),
            CalibrationServices{ .electricalIdent = *calibration.electricalIdent, .motorAlignment = *calibration.motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = calibration.confirmedCurrentOffsets, .mapCogging = true, .standstill = calibration.rotorStandstill, .identifyFluxLinkage = true, .indexSearch = *calibration.indexSearch },
            *platformFaultNotifier,
            configData,
            ControlMode::OuterLoopArgs{
                .maxCurrent = hardware.MaxCurrentSupported(),
                .baseFrequency = hardware.BaseFrequency(),
                .lowPriorityInterrupt = hardware.LowPriorityInterrupt() });
        controlMode->Recalibration().RestoreStoredModel();
        faultHistory.emplace(storage.Nvm(), [this]()
            {
                controlMode->HoldAsyncWork();
            },
            [this]()
            {
                controlMode->ReleaseAsyncWork();
            });
        supervision.AttachPeakCurrent();
        controlMode->OnFaultEntered([this](state_machine::FaultCode code)
            {
                RecordFault(code);
            });
        faultHistory->Start();
    }

    void Logic::StartCanServices()
    {
        canServices.emplace(CanServices::Wiring{ *canStack, *controlMode, hardware, inverter, calibration, *faultHistory, currentStream, inverterCompensation.BaseFrequency() });
        canServices->canBridge.ObserveCommands([this]()
            {
                supervision.CommandAccepted();
            });
    }

    void Logic::BroadcastFaults()
    {
        platformFaultNotifier->RegisterSecondary([this](state_machine::FaultCode code)
            {
                infra::EventDispatcherWithWeakPtr::Instance().Schedule([this, code]()
                    {
                        canServices->canBridge.BroadcastFault(code);
                    });
            });
    }

    void Logic::RecordFault(state_machine::FaultCode code)
    {
        faultHistory->Record(static_cast<uint8_t>(can::FocMotorCanBridge::ToCanFaultCode(code)),
            services::FaultHistory::Conditions{ static_cast<uint8_t>(state_machine::ToCanMode(controlMode->Drive().Active())), filteredBusVoltage, supervision.PeakCurrent() });
    }
}
