#include "core/state_machine/FocStateMachineCommon.hpp"
#include "core/state_machine/FocLifecycleTable.hpp"
#include "core/state_machine/LifecycleCliCommands.hpp"

namespace application
{
    FocStateMachineCommon::FocStateMachineCommon(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices)
        : terminal(terminalAndTracer.terminal)
        , tracer(terminalAndTracer.tracer)
        , calibrationContext(hardware.inverter, hardware.vdc, calibServices.fluxLinkage)
        , environment{ stateMachine, calibrationContext, nvm, nvmActivity, pendingCommand, *this, tracer }
        , calibration(environment, calibServices)
        , maintenance(environment, calibration)
        , boot(environment, calibration)
        , operation(environment, calibration)
        , context{ *this, calibration, maintenance, boot, operation, pendingCommand }
        , stateMachine(context, FocLifecycleTable::Rows())
        , stateMachineTracer(stateMachine, tracer)
        , commandRejections(stateMachine)
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
        operation.UnregisterFaultHandler();
    }

    void FocStateMachineCommon::RegisterCliIfNeeded(state_machine::TransitionPolicy transitionPolicy)
    {
        if (transitionPolicy != state_machine::TransitionPolicy::Cli)
            return;

        RegisterLifecycleCliCommands(terminal, [this]() -> FocStateMachineCommon&
            {
                return *this;
            });
        RegisterModeSpecificCli(terminal);
    }

    void FocStateMachineCommon::Boot()
    {
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
        return pendingCommand.Pending() || nvmActivity.InFlight() || operation.HasPendingFault() || stateMachine.Is<state_machine::Calibrating>() || calibration.IsRunning() || HasModeSpecificWorkPending();
    }

    bool FocStateMachineCommon::HasPartialCalibration() const
    {
        return stateMachine.Is<state_machine::Idle>() && calibrationContext.HasPartial();
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

    void FocStateMachineCommon::CmdReAlign(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        Dispatch(state_machine::ReAlign{ state_machine::CommandCallback{ onDone } });
    }

    state_machine::CommandResult FocStateMachineCommon::CmdReserveExternalCalibration()
    {
        return ToCommandResult(Dispatch(state_machine::ReserveExternalCalibration{}));
    }

    void FocStateMachineCommon::CmdCompleteExternalCalibration(const services::CalibrationData& data, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        Dispatch(state_machine::CompleteExternalCalibration{ data, state_machine::CommandCallback{ onDone } });
    }

    void FocStateMachineCommon::CmdSetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        Dispatch(state_machine::SetFluxLinkage{ fluxLinkage, state_machine::CommandCallback{ onDone } });
    }

    foc::Weber FocStateMachineCommon::ActiveFluxLinkage() const
    {
        return calibrationContext.ActiveFluxLinkage();
    }

    void FocStateMachineCommon::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        operation.RegisterReadyHandler(onReady);
    }

    void FocStateMachineCommon::MarkProvisionalControlApplied()
    {
        provisionalControlApplied = true;
    }

    // A committed calibration has just been applied over the provisional plant, so there is nothing left to
    // put back.
    void FocStateMachineCommon::ProvisionalControlSuperseded()
    {
        provisionalControlApplied = false;
    }

    // A provisional model exists only to make the rotor move during identification. Whatever ends the run
    // short of a committed calibration must put back the model the drive had before it, so that the loops
    // are never left tuned for a plant nobody measured.
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

    void FocStateMachineCommon::ApplyModeSpecificCalibration(const services::CalibrationData& /*data*/)
    {}

    bool FocStateMachineCommon::HasValidModeSpecificCalibration(const services::CalibrationData& /*data*/) const
    {
        return true;
    }

    void FocStateMachineCommon::PrepareForEnabled()
    {}

    void FocStateMachineCommon::AbortModeSpecificServices()
    {}

    bool FocStateMachineCommon::HasModeSpecificWorkPending() const
    {
        return false;
    }

    void FocStateMachineCommon::RegisterModeSpecificCli(services::TerminalWithStorage& /*terminal*/)
    {}

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

    drivers::ThreePhaseInverter& FocStateMachineCommon::GetInverter()
    {
        return calibrationContext.GetInverter();
    }

    foc::Volts FocStateMachineCommon::GetVdc() const
    {
        return calibrationContext.GetVdc();
    }

    const services::CalibrationData& FocStateMachineCommon::GetCalibration() const
    {
        return calibrationContext.Data();
    }

    foc::Weber FocStateMachineCommon::EffectiveFluxLinkage(const services::CalibrationData& data) const
    {
        return calibrationContext.EffectiveFluxLinkage(data);
    }

    bool FocStateMachineCommon::ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, float bandwidth, foc::Weber fluxLinkage)
    {
        return calibrationContext.ApplyModel(resistance, inductance, polePairs, bandwidth, fluxLinkage, GetFoc(), CurrentTunable());
    }

    state_machine::CommandResult FocStateMachineCommon::ToCommandResult(services::DispatchResult result)
    {
        if (result == services::DispatchResult::rejected || result == services::DispatchResult::forbidden)
            return state_machine::CommandResult::rejected;

        if (result == services::DispatchResult::queued)
            return state_machine::CommandResult::queued;

        return state_machine::CommandResult::ok;
    }
}
