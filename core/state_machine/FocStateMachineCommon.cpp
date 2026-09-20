#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    namespace
    {
        bool WasActive(const state_machine::State& state)
        {
            return std::holds_alternative<state_machine::Enabled>(state) || std::holds_alternative<state_machine::Calibrating>(state);
        }
    }

    FocStateMachineCommon::Observer::Observer(StateMachine& subject, FocStateMachineCommon& owner)
        : services::StateMachineObserver<state_machine::State, state_machine::Event>(subject)
        , owner(owner)
    {}

    void FocStateMachineCommon::Observer::StateChanged(StateId, const state_machine::Event&, StateId to)
    {
        owner.OnStateChanged(to);
    }

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
        , stateMachineTracer(stateMachine, terminalAndTracer.tracer)
    {
        AddCalibrationRows();
        AddCalibrationCompletionRows();
        AddOperationRows();
        AddSafetyRows();
        AddMaintenanceRows();
        AddBootRows();
    }

    template<class Stopped>
    void FocStateMachineCommon::AddCalibrationEntryRows()
    {
        stateMachine.Add<Stopped, state_machine::Calibrate, state_machine::Calibrating>(
            [this](const Stopped&, const state_machine::Calibrate&)
            {
                return !HasPendingCommand();
            },
            [this](Stopped&, const state_machine::Calibrate& command)
            {
                return BeginCalibration(command);
            });

        stateMachine.Add<Stopped, state_machine::ReAlign, state_machine::Calibrating>(
            [this](const Stopped&, const state_machine::ReAlign&)
            {
                return !HasPendingCommand() && HasValidCalibration();
            },
            [this](Stopped&, const state_machine::ReAlign& command)
            {
                return BeginReAlign(command);
            });

        stateMachine.Add<Stopped, state_machine::ReserveExternalCalibration, state_machine::Calibrating>(
            [this](const Stopped&, const state_machine::ReserveExternalCalibration&)
            {
                return !HasPendingAsyncWork();
            },
            [](Stopped&, const state_machine::ReserveExternalCalibration&)
            {
                return state_machine::Calibrating{ state_machine::CalibrationStep::polePairs, {}, true };
            });
    }

    template<class Active>
    void FocStateMachineCommon::AddEmergencyStopRows()
    {
        stateMachine.Add<Active, state_machine::EmergencyStop, state_machine::Ready>(
            [this](const Active&, const state_machine::EmergencyStop&)
            {
                return HasValidCalibration();
            },
            [this](Active&, const state_machine::EmergencyStop&)
            {
                return StopToReady();
            });

        stateMachine.Add<Active, state_machine::EmergencyStop, state_machine::Idle>(nullptr, [this](Active&, const state_machine::EmergencyStop&)
            {
                return StopToIdle();
            });
    }

    void FocStateMachineCommon::AddCalibrationRows()
    {
        AddCalibrationEntryRows<state_machine::Idle>();
        AddCalibrationEntryRows<state_machine::Ready>();

        stateMachine.AddInternal<state_machine::Calibrating, state_machine::CompleteExternalCalibration>(
            [this](state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command)
            {
                pendingCommandCallback = command.onDone;
                calibrating.pendingData = command.data;
                StartAlignmentOnly(calibrating);
            },
            [this](const state_machine::Calibrating&, const state_machine::CompleteExternalCalibration& command)
            {
                if (CalibrationContext::HasFiniteElectricalParameters(command.data))
                    return true;

                tracer.Trace() << "[SM] External calibration rejected: implausible electrical data";
                return false;
            });

        stateMachine.AddInternal<state_machine::Calibrating, state_machine::RunCalibrationSequence>([this](state_machine::Calibrating& calibrating, const state_machine::RunCalibrationSequence&)
            {
                StartCalibrationSequence(calibrating);
            });

        stateMachine.AddInternal<state_machine::Calibrating, state_machine::RunAlignmentOnly>([this](state_machine::Calibrating& calibrating, const state_machine::RunAlignmentOnly&)
            {
                StartAlignmentOnly(calibrating);
            });

        stateMachine.AddInternal<state_machine::Calibrating, state_machine::CalibrationStepChanged>([](state_machine::Calibrating& calibrating, const state_machine::CalibrationStepChanged& event)
            {
                calibrating.step = event.step;
            });

        stateMachine.AddInternal<state_machine::Calibrating, state_machine::AlignmentSucceeded>([this](state_machine::Calibrating& calibrating, const state_machine::AlignmentSucceeded& event)
            {
                OnAlignmentSucceeded(calibrating, event.angle);
            });

        stateMachine.AddInternal<state_machine::Calibrating, state_machine::MechanicalParametersIdentified>(
            [this](state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
            {
                OnMechanicalParametersIdentified(calibrating, event);
            },
            [](const state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified&)
            {
                return calibrating.step == state_machine::CalibrationStep::frictionAndInertia;
            });
    }

    void FocStateMachineCommon::AddCalibrationCompletionRows()
    {
        stateMachine.Add<state_machine::Calibrating, state_machine::CalibrationStepFailed, state_machine::Fault>(nullptr, [this](state_machine::Calibrating&, const state_machine::CalibrationStepFailed&)
            {
                CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
                return BuildFault(state_machine::FaultCode::calibrationFailed, true);
            });

        stateMachine.Add<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Fault>(
            [](const state_machine::Calibrating&, const state_machine::CalibrationSaved& event)
            {
                return event.status != services::NvmStatus::Ok;
            },
            [this](state_machine::Calibrating&, const state_machine::CalibrationSaved&)
            {
                CompletePendingCommand(state_machine::CommandResult::nvmFailed);
                return BuildFault(state_machine::FaultCode::calibrationFailed, true);
            });

        stateMachine.Add<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Ready>(
            [](const state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
            {
                return calibrating.pendingData.stage == services::CalibrationStage::complete;
            },
            [this](state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
            {
                return CompleteCalibration(calibrating);
            });

        stateMachine.Add<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Idle>(nullptr, [this](state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
            {
                return CompletePartialCalibration(calibrating);
            });
    }

    void FocStateMachineCommon::AddOperationRows()
    {
        stateMachine.Add<state_machine::Ready, state_machine::Enable, state_machine::Enabled>(
            [this](const state_machine::Ready& ready, const state_machine::Enable&)
            {
                if (faultController.IsLatched())
                    return false;

                if (!ready.rotorReferenceValid)
                    tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";

                return ready.rotorReferenceValid;
            },
            [this](state_machine::Ready&, const state_machine::Enable&)
            {
                return BuildEnabled();
            });

        stateMachine.Add<state_machine::Enabled, state_machine::Disable, state_machine::Ready>(nullptr, [this](state_machine::Enabled&, const state_machine::Disable&)
            {
                GetFocControl().Stop();
                faultController.ResetClearCount();
                return BuildReady();
            });
    }

    void FocStateMachineCommon::AddSafetyRows()
    {
        stateMachine.AddFromAny<state_machine::FaultDetected, state_machine::Fault>(nullptr, [this](state_machine::State& from, const state_machine::FaultDetected& event)
            {
                return BuildFault(event.code, WasActive(from));
            });

        stateMachine.Add<state_machine::Fault, state_machine::ClearFault, state_machine::Ready>(
            [this](const state_machine::Fault&, const state_machine::ClearFault&)
            {
                return HasValidCalibration();
            },
            [this](state_machine::Fault&, const state_machine::ClearFault&)
            {
                tracer.Trace() << "[SM] Fault cleared";
                return BuildReady();
            });

        stateMachine.Add<state_machine::Fault, state_machine::ClearFault, state_machine::Idle>(nullptr, [this](state_machine::Fault&, const state_machine::ClearFault&)
            {
                tracer.Trace() << "[SM] Fault cleared";
                return state_machine::Idle{};
            });

        AddEmergencyStopRows<state_machine::Enabled>();
        AddEmergencyStopRows<state_machine::Calibrating>();

        stateMachine.AddInternal<state_machine::Idle, state_machine::EmergencyStop>([this](state_machine::Idle&, const state_machine::EmergencyStop&)
            {
                StopWithoutTransition();
            });

        stateMachine.AddInternal<state_machine::Ready, state_machine::EmergencyStop>([this](state_machine::Ready&, const state_machine::EmergencyStop&)
            {
                StopWithoutTransition();
            });

        stateMachine.AddInternal<state_machine::Fault, state_machine::EmergencyStop>([this](state_machine::Fault&, const state_machine::EmergencyStop&)
            {
                StopWithoutTransition();
            });
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

    state_machine::Ready FocStateMachineCommon::BuildReady()
    {
        return state_machine::Ready{ calibrationContext.Data(), calibrationContext.IsRotorReferenceValid() };
    }

    state_machine::Enabled FocStateMachineCommon::BuildEnabled()
    {
        PrepareForEnabled();
        return state_machine::Enabled{};
    }

    state_machine::Fault FocStateMachineCommon::BuildFault(state_machine::FaultCode code, bool wasActive)
    {
        lastFaultCode = code;
        faultController.EnterFault();

        if (wasActive)
            GetFocControl().Stop();

        AbortCalibrationServices();
        CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return state_machine::Fault{ code };
    }

    void FocStateMachineCommon::StopWithoutTransition()
    {
        tracer.Trace() << "[SM] Emergency stop";
        AbortCalibrationServices();
        CompletePendingCommand(state_machine::CommandResult::abortedByFault);
    }

    state_machine::Idle FocStateMachineCommon::StopToIdle()
    {
        StopWithoutTransition();
        return state_machine::Idle{};
    }

    state_machine::Ready FocStateMachineCommon::StopToReady()
    {
        StopWithoutTransition();
        return BuildReady();
    }

    void FocStateMachineCommon::OnStateChanged(StateId to)
    {
        if (to.Is<state_machine::Enabled>())
            GetFocControl().Start();

        if (to.Is<state_machine::Ready>() && readyHandler != nullptr)
            readyHandler();

        if (deferredCompletion.has_value())
        {
            auto result = *deferredCompletion;
            deferredCompletion.reset();
            CompletePendingCommand(result);
        }
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

    void FocStateMachineCommon::AddBootRows()
    {
        stateMachine.AddInternal<state_machine::Idle, state_machine::BootValidityChecked>([this](state_machine::Idle&, const state_machine::BootValidityChecked& event)
            {
                if (!event.valid)
                {
                    tracer.Trace() << "[SM] NVM invalid, starting in Idle";
                    return;
                }

                bootCheckInFlight = true;
                nvm.LoadCalibration(calibrationContext.MutableData(), [this](services::NvmStatus status)
                    {
                        OnBootCalibrationLoaded(status);
                    });
            });

        stateMachine.AddInternal<state_machine::Idle, state_machine::BootCalibrationLoaded>([this](state_machine::Idle&, const state_machine::BootCalibrationLoaded& event)
            {
                if (event.status != services::NvmStatus::Ok)
                    tracer.Trace() << "[SM] NVM load failed, starting in Idle";
                else if (!HasValidCalibration())
                    tracer.Trace() << "[SM] NVM data incomplete, starting in Idle";
                else
                {
                    tracer.Trace() << "[SM] Electrical parameters restored; run alignment before enabling";
                    calibrationContext.Apply(GetFoc(), CurrentTunable());
                    ApplyModeSpecificCalibration(calibrationContext.Data());
                }
            });
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
