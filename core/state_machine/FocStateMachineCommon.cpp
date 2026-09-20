#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    FocStateMachineCommon::FocStateMachineCommon(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices)
        : terminal(terminalAndTracer.terminal)
        , tracer(terminalAndTracer.tracer)
        , nvm(nvm)
        , calibrationContext(hardware.inverter, hardware.vdc, calibServices.fluxLinkage)
        , calibrationOrchestrator(calibServices.electricalIdent, calibServices.motorAlignment, terminalAndTracer.tracer)
        , stateMachine(*this, Rows())
        , stateMachineTracer(stateMachine, terminalAndTracer.tracer)
    {
        RegisterEnteredHooks();
    }

    void FocStateMachineCommon::RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier)
    {
        faultController.Register(
            faultNotifier,
            [this](state_machine::FaultCode)
            {
                GetFocControl().Stop();
            },
            [this](state_machine::FaultCode code)
            {
                Dispatch(state_machine::FaultDetected{ code });
            });
    }

    void FocStateMachineCommon::ReleaseExternalResources()
    {
        AbortCalibrationServices();
        faultController.Unregister();
    }

    void FocStateMachineCommon::AbortCalibrationServices()
    {
        calibrationOrchestrator.Abort();
        AbortModeSpecificServices();
    }

    void FocStateMachineCommon::AbortModeSpecificServices()
    {}

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

    const state_machine::State& FocStateMachineCommon::CurrentState() const
    {
        return stateMachine.CurrentState();
    }

    state_machine::FaultCode FocStateMachineCommon::LastFaultCode() const
    {
        return lastFaultCode;
    }

    bool FocStateMachineCommon::HasPendingAsyncWork() const
    {
        return HasPendingCommand() || bootCheckInFlight || stateMachine.Is<state_machine::Calibrating>() || calibrationOrchestrator.IsRunning();
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
        if (ToCommandResult(Dispatch(state_machine::Calibrate{ onDone })) != state_machine::CommandResult::ok)
            onDone(state_machine::CommandResult::rejected);
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
        if (!stateMachine.Is<state_machine::Fault>())
            return state_machine::CommandResult::rejected;

        if (!faultController.TryClear())
        {
            tracer.Trace() << "[SM] Fault clear refused, retry limit reached; reset required";
            return state_machine::CommandResult::rejected;
        }

        return ToCommandResult(Dispatch(state_machine::ClearFault{}));
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEmergencyStop()
    {
        GetFocControl().Stop();
        Dispatch(state_machine::EmergencyStop{});
        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        readyHandler = onReady;
    }

    void FocStateMachineCommon::ApplyModeSpecificCalibration(const services::CalibrationData& /*data*/)
    {}

    bool FocStateMachineCommon::HasValidModeSpecificCalibration(const services::CalibrationData& /*data*/) const
    {
        return true;
    }

    void FocStateMachineCommon::PrepareForEnabled()
    {}

    void FocStateMachineCommon::RegisterModeSpecificCli(services::TerminalWithStorage& /*terminal*/)
    {}

    bool FocStateMachineCommon::IsEnableAllowed(const state_machine::Ready& ready)
    {
        if (faultController.IsLatched())
            return false;

        if (!ready.rotorReferenceValid)
            tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";

        return ready.rotorReferenceValid;
    }

    state_machine::Ready FocStateMachineCommon::DisableToReady()
    {
        GetFocControl().Stop();
        faultController.ResetClearCount();
        return BuildReady();
    }

    state_machine::Ready FocStateMachineCommon::ClearFaultToReady()
    {
        tracer.Trace() << "[SM] Fault cleared";
        return BuildReady();
    }

    state_machine::Idle FocStateMachineCommon::ClearFaultToIdle()
    {
        tracer.Trace() << "[SM] Fault cleared";
        return state_machine::Idle{};
    }

    state_machine::Ready FocStateMachineCommon::BuildReady()
    {
        return state_machine::Ready{ calibrationContext.Data(), calibrationContext.IsRotorReferenceValid() };
    }

    state_machine::Enabled FocStateMachineCommon::BuildEnabled()
    {
        PrepareForEnabled();
        return state_machine::Enabled{};
    }

    state_machine::Fault FocStateMachineCommon::BuildFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult)
    {
        lastFaultCode = code;
        faultController.EnterFault();

        if (wasActive)
            GetFocControl().Stop();

        AbortCalibrationServices();
        CompleteAfterTransition(pendingResult);
        return state_machine::Fault{ code };
    }

    void FocStateMachineCommon::AbortActiveWork()
    {
        tracer.Trace() << "[SM] Emergency stop";
        AbortCalibrationServices();
    }

    void FocStateMachineCommon::StopWithoutTransition()
    {
        AbortActiveWork();
        CompletePendingCommand(state_machine::CommandResult::abortedByFault);
    }

    state_machine::Idle FocStateMachineCommon::StopToIdle()
    {
        AbortActiveWork();
        CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return state_machine::Idle{};
    }

    state_machine::Ready FocStateMachineCommon::StopToReady()
    {
        AbortActiveWork();
        CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return BuildReady();
    }

    void FocStateMachineCommon::OnStateEntered(StateId state)
    {
        if (state.Is<state_machine::Enabled>())
            GetFocControl().Start();

        if (deferredCompletion.has_value())
        {
            auto result = *deferredCompletion;
            deferredCompletion.reset();
            CompletePendingCommand(result);
        }

        if (state.Is<state_machine::Ready>() && readyHandler != nullptr)
            readyHandler();
    }

    void FocStateMachineCommon::CompleteAfterTransition(state_machine::CommandResult result)
    {
        deferredCompletion = result;
    }

    void FocStateMachineCommon::CompletePendingCommand(state_machine::CommandResult result)
    {
        if (pendingCommandCallback != nullptr)
            pendingCommandCallback(result);
    }

    bool FocStateMachineCommon::HasPendingCommand() const
    {
        return pendingCommandCallback != nullptr;
    }

    bool FocStateMachineCommon::HasValidCalibration() const
    {
        return calibrationContext.IsComplete(HasValidModeSpecificCalibration(calibrationContext.Data()));
    }

    services::DispatchResult FocStateMachineCommon::Dispatch(const state_machine::Event& event)
    {
        return stateMachine.Dispatch(event);
    }

    state_machine::CommandResult FocStateMachineCommon::ToCommandResult(services::DispatchResult result)
    {
        if (result == services::DispatchResult::rejected || result == services::DispatchResult::forbidden)
            return state_machine::CommandResult::rejected;

        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::Boot()
    {
        stateMachine.Start<state_machine::Idle>();

        bootCheckInFlight = true;
        nvm.IsCalibrationValid([this](bool valid)
            {
                OnBootValidityChecked(valid);
            });
    }

    void FocStateMachineCommon::ContinueBoot(bool valid)
    {
        if (!valid)
        {
            tracer.Trace() << "[SM] NVM invalid, starting in Idle";
            return;
        }

        bootCheckInFlight = true;
        nvm.LoadCalibration(calibrationContext.MutableData(), [this](services::NvmStatus status)
            {
                OnBootCalibrationLoaded(status);
            });
    }

    void FocStateMachineCommon::FinishBoot(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
            tracer.Trace() << "[SM] NVM load failed, starting in Idle";
        else if (!HasValidCalibration())
            tracer.Trace() << "[SM] NVM data incomplete, starting in Idle";
        else
        {
            tracer.Trace() << "[SM] Electrical parameters restored; run alignment before enabling";
            calibrationContext.Apply(GetFoc(), CurrentTunable());
            ApplyModeSpecificCalibration(calibrationContext.Data());
        }
    }

    void FocStateMachineCommon::OnBootValidityChecked(bool valid)
    {
        bootCheckInFlight = false;
        Dispatch(state_machine::BootValidityChecked{ valid });
    }

    void FocStateMachineCommon::OnBootCalibrationLoaded(services::NvmStatus status)
    {
        bootCheckInFlight = false;
        Dispatch(state_machine::BootCalibrationLoaded{ status });
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
}
