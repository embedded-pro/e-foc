#include "core/state_machine/OperationFlow.hpp"

namespace application
{
    OperationFlow::OperationFlow(LifecycleMachine& machine, CalibrationFlow& calibration, PendingCommand& pending, ModeHooks& mode, services::Tracer& tracer)
        : machine(machine)
        , calibration(calibration)
        , pending(pending)
        , mode(mode)
        , tracer(tracer)
    {}

    void OperationFlow::RegisterFaultHandler(state_machine::FaultNotifier& notifier)
    {
        faultController.Register(
            notifier,
            [this](state_machine::FaultCode)
            {
                mode.GetFocControl().Stop();
            },
            [this](state_machine::FaultCode code)
            {
                machine.Dispatch(state_machine::FaultDetected{ code });
            });
    }

    void OperationFlow::UnregisterFaultHandler()
    {
        faultController.Unregister();
    }

    state_machine::FaultCode OperationFlow::LastFaultCode() const
    {
        return lastFaultCode;
    }

    bool OperationFlow::IsEnableAllowed(const state_machine::Ready& ready)
    {
        if (faultController.IsLatched())
            return false;

        if (!ready.rotorReferenceValid)
            tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";

        return ready.rotorReferenceValid;
    }

    state_machine::Enabled OperationFlow::BuildEnabled()
    {
        mode.PrepareForEnabled();
        return state_machine::Enabled{};
    }

    state_machine::Ready OperationFlow::DisableToReady()
    {
        mode.GetFocControl().Stop();
        faultController.ResetClearCount();
        return calibration.ReadyState();
    }

    state_machine::Fault OperationFlow::EnterFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult)
    {
        lastFaultCode = code;
        faultController.EnterFault();

        if (wasActive)
            mode.GetFocControl().Stop();

        calibration.Abort();
        pending.CompleteAfterTransition(pendingResult);
        return state_machine::Fault{ code };
    }

    bool OperationFlow::TryClearFault()
    {
        if (faultController.TryClear())
            return true;

        tracer.Trace() << "[SM] Fault clear refused, retry limit reached; reset required";
        return false;
    }

    state_machine::Ready OperationFlow::ClearFaultToReady()
    {
        tracer.Trace() << "[SM] Fault cleared";
        return calibration.ReadyState();
    }

    state_machine::Idle OperationFlow::ClearFaultToIdle()
    {
        tracer.Trace() << "[SM] Fault cleared";
        return state_machine::Idle{};
    }

    void OperationFlow::AbortActiveWork()
    {
        tracer.Trace() << "[SM] Emergency stop";
        calibration.Abort();
    }

    void OperationFlow::StopWithoutTransition()
    {
        AbortActiveWork();
        pending.Complete(state_machine::CommandResult::abortedByFault);
    }

    state_machine::Idle OperationFlow::StopToIdle()
    {
        AbortActiveWork();
        pending.CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return state_machine::Idle{};
    }

    state_machine::Ready OperationFlow::StopToReady()
    {
        AbortActiveWork();
        pending.CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return calibration.ReadyState();
    }

    void OperationFlow::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        readyHandler = onReady;
    }

    void OperationFlow::StateEntered(StateId state)
    {
        if (state.Is<state_machine::Enabled>())
            mode.GetFocControl().Start();

        pending.FlushDeferred();

        if (state.Is<state_machine::Ready>() && readyHandler != nullptr)
            readyHandler();
    }
}
