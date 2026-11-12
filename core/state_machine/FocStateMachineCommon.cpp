#include "core/state_machine/FocStateMachineCommon.hpp"
#include "core/state_machine/FocLifecycleTable.hpp"
#include "e-foc-protocol/foc/ParameterValidation.hpp"

namespace application
{
    FocStateMachineCommon::FocStateMachineCommon(
        services::Tracer& tracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        NvmActivity& nvmActivity)
        : tracer(tracer)
        , calibrationContext(hardware.inverter, hardware.vdc, calibServices.fluxLinkage, calibServices.currentOffsets, hardware.dutyUpdateDelayPeriods)
        , nvmActivity(nvmActivity)
        , environment{ stateMachine, calibrationContext, nvm, nvmActivity, pendingCommand, *this, tracer }
        , calibration(environment, calibServices)
        , maintenance(environment, calibration)
        , boot(environment, calibration)
        , operation(environment, calibration, calibServices)
        , context{ *this, calibration, maintenance, boot, operation, pendingCommand }
        , stateMachine(context, FocLifecycleTable::Table())
        , stateMachineTracer(stateMachine, tracer)
        , commandRejections(stateMachine)
        , commissioning(stateMachine)
    {
        FocLifecycleTable::RegisterEnteredHooks(stateMachine);
    }

    void FocStateMachineCommon::RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier)
    {
        operation.RegisterFaultHandler(faultNotifier);
    }

    void FocStateMachineCommon::ReleaseExternalResources()
    {
        calibration.Abort();
        operation.AbortEnablePreparation();
        operation.UnregisterFaultHandler();
    }

    void FocStateMachineCommon::Boot(RotorReference rotorReference)
    {
        calibrationContext.SetRotorReferenceValid(rotorReference != RotorReference::lost);
        calibrationContext.SetHomed(rotorReference == RotorReference::keptHomed);
        stateMachine.Start<state_machine::Idle>();
        boot.Begin();
    }

    const state_machine::State& FocStateMachineCommon::CurrentState() const
    {
        return stateMachine.CurrentState();
    }

    state_machine::FaultCode FocStateMachineCommon::LastFaultCode() const
    {
        return operation.LastFaultCode();
    }

    bool FocStateMachineCommon::HasPendingAsyncWork() const
    {
        return pendingCommand.Pending() || nvmActivity.InFlight() || operation.HasPendingFault() || operation.IsPreparingEnable() || stateMachine.Is<state_machine::Calibrating>() || calibration.IsRunning() || HasModeSpecificWorkPending();
    }

    bool FocStateMachineCommon::HasPartialCalibration() const
    {
        return stateMachine.Is<state_machine::Idle>() && calibrationContext.HasPartial();
    }

    void FocStateMachineCommon::RequireHoming(bool required)
    {
        operation.RequireHoming(required);
    }

    CommissioningCommands& FocStateMachineCommon::Commissioning()
    {
        return commissioning;
    }

    const CalibrationContext& FocStateMachineCommon::Calibration() const
    {
        return calibrationContext;
    }

    const FocStateMachineCommon::StateMachine& FocStateMachineCommon::TransitionTable() const
    {
        return stateMachine;
    }

    void FocStateMachineCommon::CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        Dispatch(state_machine::Calibrate{ state_machine::CommandCallback{ onDone } });
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEnable()
    {
        const auto dispatched = ToCommandResult(Dispatch(state_machine::Enable{}));

        if (dispatched != state_machine::CommandResult::ok)
            return dispatched;

        if (stateMachine.Is<state_machine::Fault>())
            return state_machine::CommandResult::abortedByFault;

        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::CmdEnableAsync(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        Dispatch(state_machine::Enable{ state_machine::CommandCallback{ onDone } });
    }

    state_machine::CommandResult FocStateMachineCommon::CmdDisable()
    {
        return ToCommandResult(Dispatch(state_machine::Disable{}));
    }

    state_machine::CommandResult FocStateMachineCommon::CmdClearFault()
    {
        if (!stateMachine.Is<state_machine::Fault>())
            return state_machine::CommandResult::rejected;

        const auto refusal = operation.EvaluateClearFault();
        if (refusal != application::ClearRefusal::none)
        {
            operation.TraceFaultClearRefused(refusal);
            return refusal == application::ClearRefusal::retryLimitReached ? state_machine::CommandResult::rejected : state_machine::CommandResult::faultConditionActive;
        }

        return ToCommandResult(Dispatch(state_machine::ClearFault{}));
    }

    void FocStateMachineCommon::CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        Dispatch(state_machine::ClearCalibration{ state_machine::CommandCallback{ onDone } });
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEmergencyStop()
    {
        GetFocControl().Stop();
        Dispatch(state_machine::EmergencyStop{});
        return state_machine::CommandResult::ok;
    }

    state_machine::CommandResult FocStateMachineCommon::CmdQuickStop()
    {
        if (std::holds_alternative<state_machine::Enabled>(CurrentState()) && !operation.CanBeginQuickStop())
            return state_machine::CommandResult::rejected;
        return ToCommandResult(Dispatch(state_machine::QuickStop{}));
    }

    void FocStateMachineCommon::SetQuickStopDeceleration(foc::RadiansPerSecondSquared deceleration)
    {
        quickStopDeceleration = deceleration;
    }

    foc::RadiansPerSecondSquared FocStateMachineCommon::QuickStopDeceleration() const
    {
        return quickStopDeceleration;
    }

    void FocStateMachineCommon::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        operation.RegisterReadyHandler(onReady);
    }

    void FocStateMachineCommon::RegisterFaultEnteredHandler(const infra::Function<void(state_machine::FaultCode)>& onFaultEntered)
    {
        operation.RegisterFaultEnteredHandler(onFaultEntered);
    }

    void FocStateMachineCommon::MarkProvisionalControlApplied()
    {
        provisionalControlApplied = true;
    }

    bool FocStateMachineCommon::ApplyProvisionalCurrentControl(const services::CalibrationData& pending)
    {
        if (!foc::IsFinitePositive(pending.rPhase) || !foc::IsFinitePositive(pending.lD) || pending.polePairs == 0)
            return false;

        MarkProvisionalControlApplied();
        return ApplyElectricalModel(foc::Ohm{ pending.rPhase }, foc::MilliHenry{ pending.lD }, pending.polePairs, calibrationContext.EffectiveFluxLinkage(pending), foc::Ampere{ services::CurrentNoiseAmpere(pending.currentNoiseMilliAmpere) });
    }

    // A committed calibration replaces the provisional plant; nothing is left to restore.
    void FocStateMachineCommon::ProvisionalControlSuperseded()
    {
        provisionalControlApplied = false;
    }

    // Anything short of a committed calibration must put back the model the drive had before the run.
    void FocStateMachineCommon::RestoreControlAfterProvisionalIdentification()
    {
        if (!provisionalControlApplied)
            return;

        provisionalControlApplied = false;

        if (calibration.HasValidCalibration())
        {
            tracer.Trace() << "[SM] Provisional identification model discarded, previous calibration restored";
            calibrationContext.Apply(GetFoc(), CurrentTunable());
            ApplyModeSpecificCalibration(calibrationContext.Data());
        }
        else
            tracer.Trace() << "[SM] Provisional identification model discarded, no calibration held";
    }

    void FocStateMachineCommon::SaveCalibration(state_machine::Calibrating& calibrating)
    {
        calibration.Save(calibrating);
    }

    services::DispatchResult FocStateMachineCommon::Dispatch(const state_machine::Event& event)
    {
        return stateMachine.Dispatch(event);
    }

    services::Tracer& FocStateMachineCommon::GetTracer()
    {
        return tracer;
    }

    const services::CalibrationData& FocStateMachineCommon::GetCalibration() const
    {
        return calibrationContext.Data();
    }

    foc::Weber FocStateMachineCommon::EffectiveFluxLinkage(const services::CalibrationData& data) const
    {
        return calibrationContext.EffectiveFluxLinkage(data);
    }

    bool FocStateMachineCommon::ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, foc::Weber fluxLinkage, foc::Ampere currentNoise)
    {
        return calibrationContext.ApplyModel(resistance, inductance, polePairs, fluxLinkage, currentNoise, GetFoc(), CurrentTunable());
    }
}
