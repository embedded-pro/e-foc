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
        , calibration(stateMachine, calibrationContext, nvm, pendingCommand, *this, tracer, calibServices.electricalIdent, calibServices.motorAlignment)
        , maintenance(stateMachine, calibrationContext, nvm, pendingCommand, *this, tracer, calibration)
        , boot(stateMachine, calibrationContext, nvm, *this, tracer, calibration)
        , operation(stateMachine, calibration, pendingCommand, *this, tracer)
        , context{ *this, calibration, maintenance, boot, operation, pendingCommand }
        , stateMachine(context, FocLifecycleTable::Rows())
        , stateMachineTracer(stateMachine, tracer)
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
        return pendingCommand.Pending() || boot.InFlight() || stateMachine.Is<state_machine::Calibrating>() || calibration.IsRunning() || HasModeSpecificWorkPending();
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
        DispatchCommand(state_machine::Calibrate{ onDone }, onDone);
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEnable()
    {
        if (ToCommandResult(Dispatch(state_machine::Enable{})) != state_machine::CommandResult::ok)
            return state_machine::CommandResult::rejected;

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
        if (!stateMachine.Is<state_machine::Fault>() || !operation.TryClearFault())
            return state_machine::CommandResult::rejected;

        return ToCommandResult(Dispatch(state_machine::ClearFault{}));
    }

    void FocStateMachineCommon::CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        DispatchCommand(state_machine::ClearCalibration{ onDone }, onDone);
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEmergencyStop()
    {
        GetFocControl().Stop();
        Dispatch(state_machine::EmergencyStop{});
        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::CmdReAlign(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        DispatchCommand(state_machine::ReAlign{ onDone }, onDone);
    }

    state_machine::CommandResult FocStateMachineCommon::CmdReserveExternalCalibration()
    {
        return ToCommandResult(Dispatch(state_machine::ReserveExternalCalibration{}));
    }

    void FocStateMachineCommon::CmdCompleteExternalCalibration(const services::CalibrationData& data, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        DispatchCommand(state_machine::CompleteExternalCalibration{ data, onDone }, onDone);
    }

    void FocStateMachineCommon::CmdSetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        DispatchCommand(state_machine::SetFluxLinkage{ fluxLinkage, onDone }, onDone);
    }

    foc::Weber FocStateMachineCommon::ActiveFluxLinkage() const
    {
        return calibrationContext.ActiveFluxLinkage();
    }

    void FocStateMachineCommon::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        operation.RegisterReadyHandler(onReady);
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

    void FocStateMachineCommon::ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, float bandwidth, foc::Weber fluxLinkage)
    {
        calibrationContext.ApplyModel(resistance, inductance, polePairs, bandwidth, fluxLinkage, GetFoc(), CurrentTunable());
    }

    void FocStateMachineCommon::DispatchCommand(const state_machine::Event& event, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (ToCommandResult(Dispatch(event)) != state_machine::CommandResult::ok)
            onDone(state_machine::CommandResult::rejected);
    }

    state_machine::CommandResult FocStateMachineCommon::ToCommandResult(services::DispatchResult result)
    {
        if (result == services::DispatchResult::rejected || result == services::DispatchResult::forbidden)
            return state_machine::CommandResult::rejected;

        return state_machine::CommandResult::ok;
    }
}
