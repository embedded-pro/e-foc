#include "core/state_machine/OperationFlow.hpp"

namespace application
{
    OperationFlow::OperationFlow(const LifecycleEnvironment& environment, CalibrationFlow& calibration)
        : env(environment)
        , calibration(calibration)
    {}

    void OperationFlow::RegisterFaultHandler(state_machine::FaultNotifier& notifier)
    {
        faultController.Register(
            notifier,
            [this](state_machine::FaultCode code)
            {
                faultController.LatchFromInterrupt(code);
                env.mode.GetFocControl().Stop();
            },
            [this](state_machine::FaultCode)
            {
                DispatchPendingFault();
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

    bool OperationFlow::IsEnableAllowed(const state_machine::Ready& ready) const
    {
        if (faultController.IsLatched())
            return false;

        if (!ready.rotorReferenceValid)
            env.tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";

        return ready.rotorReferenceValid;
    }

    state_machine::Enabled OperationFlow::BuildEnabled()
    {
        env.mode.PrepareForEnabled();
        return state_machine::Enabled{};
    }

    state_machine::Ready OperationFlow::DisableToReady()
    {
        env.mode.GetFocControl().Stop();
        faultController.ResetClearCount();
        return calibration.ReadyState();
    }

    state_machine::Fault OperationFlow::EnterFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult)
    {
        if (!faultController.IsRecorded())
            lastFaultCode = code;
        else
            env.tracer.Trace() << "[SM] Further fault while faulted; keeping the first code";

        faultController.EnterFault();

        if (wasActive)
            env.mode.GetFocControl().Stop();

        calibration.Abort();
        env.pending.CompleteAfterTransition(pendingResult);
        return state_machine::Fault{ lastFaultCode };
    }

    bool OperationFlow::CanClearFault() const
    {
        return faultController.CanClear();
    }

    void OperationFlow::TraceFaultClearRefused() const
    {
        env.tracer.Trace() << "[SM] Fault clear refused, retry limit reached; reset required";
    }

    state_machine::Ready OperationFlow::ClearFaultToReady()
    {
        faultController.Clear();
        env.tracer.Trace() << "[SM] Fault cleared";
        return calibration.ReadyState();
    }

    state_machine::Idle OperationFlow::ClearFaultToIdle()
    {
        faultController.Clear();
        env.tracer.Trace() << "[SM] Fault cleared";
        return state_machine::Idle{};
    }

    void OperationFlow::AbortActiveWork()
    {
        env.tracer.Trace() << "[SM] Emergency stop";
        env.mode.GetFocControl().Stop();
        calibration.Abort();
    }

    void OperationFlow::StopWithoutTransition()
    {
        AbortActiveWork();
        env.pending.Complete(state_machine::CommandResult::abortedByFault);
    }

    state_machine::Idle OperationFlow::StopToIdle()
    {
        AbortActiveWork();
        env.pending.CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return state_machine::Idle{};
    }

    state_machine::Ready OperationFlow::StopToReady()
    {
        AbortActiveWork();
        env.pending.CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
        return calibration.ReadyState();
    }

    void OperationFlow::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        readyHandler = onReady;
    }

    void OperationFlow::StartUnlessFaulted()
    {
        // A fault delivered from an interrupt cuts the bridge there and then, but the FaultDetected event
        // carrying its transition is still queued on the event dispatcher, so the lifecycle state does not
        // show it yet. Consult the latch instead: before Start(), so no step of the enable sequence undoes
        // the cutoff that preceded it, and again after it, for a fault raised while Start() ran. The
        // transition dispatched here is queued by the table and drained before the Enable dispatch returns,
        // so CmdEnable reports abortedByFault rather than success on a drive the hardware has faulted.
        if (DispatchPendingFault())
            return;

        env.mode.GetFocControl().Start();

        DispatchPendingFault();
    }

    bool OperationFlow::DispatchPendingFault()
    {
        if (!faultController.TakePendingFault())
            return false;

        env.machine.Dispatch(state_machine::FaultDetected{ faultController.PendingCode() });
        return true;
    }

    void OperationFlow::StateEntered(StateId state)
    {
        if (state.Is<state_machine::Enabled>())
            StartUnlessFaulted();

        env.pending.FlushDeferred();

        if (state.Is<state_machine::Ready>() && readyHandler != nullptr)
            readyHandler();
    }
}
