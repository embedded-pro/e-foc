#include "core/state_machine/FocStateMachineCommon.hpp"
#include <bit>

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
    {}

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
                EnterFault(code);
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
        return currentState;
    }

    state_machine::FaultCode FocStateMachineCommon::LastFaultCode() const
    {
        return lastFaultCode;
    }

    bool FocStateMachineCommon::HasPendingAsyncWork() const
    {
        return HasPendingCommand() || bootCheckInFlight || std::holds_alternative<state_machine::Calibrating>(currentState) || calibrationOrchestrator.IsRunning();
    }

    bool FocStateMachineCommon::HasPartialCalibration() const
    {
        return std::holds_alternative<state_machine::Idle>(currentState) && calibrationContext.HasPartial();
    }

    void FocStateMachineCommon::CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (!state_machine::IsStopped(currentState) || HasPendingCommand())
        {
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        pendingCommandCallback = onDone;
        EnterCalibrating();
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEnable()
    {
        if (!std::holds_alternative<state_machine::Ready>(currentState) || faultController.IsLatched())
            return state_machine::CommandResult::rejected;

        if (!std::get<state_machine::Ready>(currentState).rotorReferenceValid)
        {
            tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";
            return state_machine::CommandResult::rejected;
        }

        EnterEnabled();

        if (std::holds_alternative<state_machine::Fault>(currentState))
            return state_machine::CommandResult::abortedByFault;

        return state_machine::CommandResult::ok;
    }

    state_machine::CommandResult FocStateMachineCommon::CmdDisable()
    {
        if (!std::holds_alternative<state_machine::Enabled>(currentState))
            return state_machine::CommandResult::rejected;

        GetFocControl().Stop();
        faultController.ResetClearCount();
        EnterReady(calibrationContext.Data());
        return state_machine::CommandResult::ok;
    }

    state_machine::CommandResult FocStateMachineCommon::CmdClearFault()
    {
        if (!std::holds_alternative<state_machine::Fault>(currentState))
            return state_machine::CommandResult::rejected;

        if (!faultController.TryClear())
        {
            tracer.Trace() << "[SM] Fault clear refused, retry limit reached; reset required";
            return state_machine::CommandResult::rejected;
        }

        tracer.Trace() << "[SM] Fault cleared";
        EnterReadyOrIdle();
        return state_machine::CommandResult::ok;
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEmergencyStop()
    {
        GetFocControl().Stop();

        tracer.Trace() << "[SM] Emergency stop";

        const bool wasActive = WasActive();

        AbortCalibrationServices();
        CompletePendingCommand(state_machine::CommandResult::abortedByFault);

        if (std::holds_alternative<state_machine::Fault>(currentState))
            return state_machine::CommandResult::ok;

        if (wasActive)
            EnterReadyOrIdle();

        return state_machine::CommandResult::ok;
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

    void FocStateMachineCommon::EnterCalibrating()
    {
        tracer.Trace() << "[SM] Entering Calibrating";
        currentState = state_machine::Calibrating{};

        auto& cal = std::get<state_machine::Calibrating>(currentState);
        cal.pendingData.fluxLinkage = calibrationContext.EffectiveFluxLinkage().Value();

        calibrationOrchestrator.Start(
            cal.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                std::get<state_machine::Calibrating>(currentState).step = step;
            },
            [this](foc::Radians angle)
            {
                OnAlignmentSucceeded(angle);
            },
            [this]
            {
                FailCalibrationStep();
            });
    }

    void FocStateMachineCommon::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        readyHandler = onReady;
    }

    void FocStateMachineCommon::EnterReady(const services::CalibrationData& data)
    {
        tracer.Trace() << "[SM] Entering Ready";
        calibrationContext.SetData(data);
        currentState = state_machine::Ready{ data, calibrationContext.IsRotorReferenceValid() };

        if (readyHandler != nullptr)
            readyHandler();
    }

    void FocStateMachineCommon::EnterReadyOrIdle()
    {
        if (HasValidCalibration())
            EnterReady(calibrationContext.Data());
        else
            currentState = state_machine::Idle{};
    }

    void FocStateMachineCommon::EnterIdleWithPartialCalibration(const services::CalibrationData& data)
    {
        tracer.Trace() << "[SM] Entering Idle, calibration incomplete for this mode";
        calibrationContext.SetData(data);
        currentState = state_machine::Idle{};
    }

    void FocStateMachineCommon::EnterEnabled()
    {
        tracer.Trace() << "[SM] Entering Enabled";
        PrepareForEnabled();

        currentState = state_machine::Enabled{};
        GetFocControl().Start();

        if (std::holds_alternative<state_machine::Fault>(currentState))
            GetFocControl().Stop();
    }

    void FocStateMachineCommon::EnterFault(state_machine::FaultCode code)
    {
        tracer.Trace() << "[SM] Entering Fault";

        const bool wasActive = WasActive();

        lastFaultCode = code;
        currentState = state_machine::Fault{ code };
        faultController.EnterFault();

        if (wasActive)
            GetFocControl().Stop();

        AbortCalibrationServices();

        CompletePendingCommand(state_machine::CommandResult::abortedByFault);
    }

    bool FocStateMachineCommon::WasActive() const
    {
        return std::holds_alternative<state_machine::Enabled>(currentState) ||
               std::holds_alternative<state_machine::Calibrating>(currentState);
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

    void FocStateMachineCommon::RunAlignmentStep()
    {
        auto& cal = std::get<state_machine::Calibrating>(currentState);

        calibrationOrchestrator.StartAlignmentOnly(
            cal.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                std::get<state_machine::Calibrating>(currentState).step = step;
            },
            [this](foc::Radians angle)
            {
                OnAlignmentSucceeded(angle);
            },
            [this]
            {
                FailCalibrationStep();
            });
    }

    void FocStateMachineCommon::OnAlignmentSucceeded(foc::Radians angle)
    {
        auto& cal = std::get<state_machine::Calibrating>(currentState);
        cal.pendingData.encoderZeroOffset = std::bit_cast<int32_t>(angle.Value());
        calibrationContext.SetRotorReferenceValid(true);

        if (cal.external)
            OnCalibrationComplete();
        else
            RunPostAlignmentStep();
    }

    void FocStateMachineCommon::FailCalibrationStep()
    {
        CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
        EnterFault(state_machine::FaultCode::calibrationFailed);
    }

    bool FocStateMachineCommon::IsCalibrating(state_machine::CalibrationStep expected) const
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return false;
        return std::get<state_machine::Calibrating>(currentState).step == expected;
    }

    void FocStateMachineCommon::CheckNvmOnBoot()
    {
        bootCheckInFlight = true;
        nvm.IsCalibrationValid([this](bool valid)
            {
                OnBootValidityChecked(valid);
            });
    }

    void FocStateMachineCommon::OnBootValidityChecked(bool valid)
    {
        if (!std::holds_alternative<state_machine::Idle>(currentState))
        {
            bootCheckInFlight = false;
            return;
        }

        if (!valid)
        {
            bootCheckInFlight = false;
            tracer.Trace() << "[SM] NVM invalid, starting in Idle";
            return;
        }

        nvm.LoadCalibration(calibrationContext.MutableData(), [this](services::NvmStatus status)
            {
                OnBootCalibrationLoaded(status);
            });
    }

    void FocStateMachineCommon::OnBootCalibrationLoaded(services::NvmStatus status)
    {
        bootCheckInFlight = false;

        if (!std::holds_alternative<state_machine::Idle>(currentState))
            return;

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

    state_machine::State& FocStateMachineCommon::GetCurrentState()
    {
        return currentState;
    }

    const state_machine::State& FocStateMachineCommon::GetCurrentState() const
    {
        return currentState;
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
