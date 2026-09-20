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
            [this](state_machine::FaultCode)
            {
                env.mode.GetFocControl().Stop();
            },
            [this](state_machine::FaultCode code)
            {
                env.machine.Dispatch(state_machine::FaultDetected{ code });
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
        lastFaultCode = code;
        faultController.EnterFault();

        if (wasActive)
            env.mode.GetFocControl().Stop();

        calibration.Abort();
        env.pending.CompleteAfterTransition(pendingResult);
        return state_machine::Fault{ code };
    }

    bool OperationFlow::TryClearFault()
    {
        if (faultController.TryClear())
            return true;

        env.tracer.Trace() << "[SM] Fault clear refused, retry limit reached; reset required";
        return false;
    }

    state_machine::Ready OperationFlow::ClearFaultToReady()
    {
        env.tracer.Trace() << "[SM] Fault cleared";
        return calibration.ReadyState();
    }

    state_machine::Idle OperationFlow::ClearFaultToIdle()
    {
        env.tracer.Trace() << "[SM] Fault cleared";
        return state_machine::Idle{};
    }

    void OperationFlow::AbortActiveWork()
    {
        env.tracer.Trace() << "[SM] Emergency stop";
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

    void OperationFlow::StateEntered(StateId state)
    {
        if (state.Is<state_machine::Enabled>())
            env.mode.GetFocControl().Start();

        env.pending.FlushDeferred();

        if (state.Is<state_machine::Ready>() && readyHandler != nullptr)
            readyHandler();
    }
}
