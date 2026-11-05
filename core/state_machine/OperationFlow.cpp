#include "core/state_machine/OperationFlow.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace application
{
    OperationFlow::OperationFlow(const LifecycleEnvironment& environment, CalibrationFlow& calibration, const CalibrationServices& services)
        : env(environment)
        , calibration(calibration)
        , offsetRefresh(services.currentOffsets, services.standstill, environment.tracer)
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

    bool OperationFlow::HasPendingFault() const
    {
        return faultController.IsPending();
    }

    bool OperationFlow::IsEnableAllowed(const state_machine::Ready& ready)
    {
        faultController.SampleCondition();

        if (faultController.IsLatched())
            return false;

        if (IsFaultConditionAsserted())
        {
            env.tracer.Trace() << "[SM] Enable rejected: board protection asserted";
            return false;
        }

        if (!ready.rotorReferenceValid)
            env.tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";

        return ready.rotorReferenceValid;
    }

    bool OperationFlow::IsOffsetRefreshDue() const
    {
        return offsetRefresh.IsDue();
    }

    bool OperationFlow::IsRefreshingOffsets() const
    {
        return offsetRefresh.IsRunning();
    }

    void OperationFlow::BeginOffsetRefresh(const state_machine::Ready& ready, const state_machine::Enable& command)
    {
        env.pending.Accept(command.onDone);

        const auto& record = ready.loadedData;
        offsetRefresh.Start(foc::PhaseCurrents{ foc::Ampere{ record.currentOffsetA }, foc::Ampere{ record.currentOffsetB }, foc::Ampere{ record.currentOffsetC } },
            [this](state_machine::OffsetRefreshOutcome outcome)
            {
                env.machine.Dispatch(state_machine::OffsetsRefreshed{ outcome });
            });
    }

    void OperationFlow::RefuseAfterOffsetRefresh(state_machine::OffsetRefreshOutcome outcome)
    {
        env.pending.Complete(outcome == state_machine::OffsetRefreshOutcome::refreshed ? state_machine::CommandResult::rejected : state_machine::CommandResult::calibrationFailed);
    }

    void OperationFlow::AbandonOffsetRefresh()
    {
        env.tracer.Trace() << "[SM] Enable abandoned: disabled while measuring the current offsets";
        offsetRefresh.Abort();
        env.pending.Complete(state_machine::CommandResult::rejected);
    }

    void OperationFlow::AbortOffsetRefresh()
    {
        offsetRefresh.Abort();
    }

    state_machine::Enabled OperationFlow::BuildEnabled(const state_machine::Enable& command)
    {
        env.pending.Accept(command.onDone);
        return BuildEnabledAfterOffsetRefresh();
    }

    state_machine::Enabled OperationFlow::BuildEnabledAfterOffsetRefresh()
    {
        env.mode.PrepareForEnabled();
        env.pending.CompleteAfterTransition(state_machine::CommandResult::ok);
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
        env.tracer.Trace() << "[SM] Fault: " << state_machine::FaultCodeName(code);

        if (!faultController.IsRecorded())
            lastFaultCode = code;
        else
            env.tracer.Trace() << "[SM] Further fault while faulted; keeping the first code";

        faultDispatched = false;
        faultController.EnterFault();
        faultController.SampleCondition();
        offsetRefresh.Abort();
        offsetRefresh.MarkStale();

        if (wasActive)
            env.mode.GetFocControl().Stop();

        calibration.Abort();
        env.pending.CompleteAfterTransition(pendingResult);
        return state_machine::Fault{ lastFaultCode };
    }

    bool OperationFlow::IsFaultConditionAsserted() const
    {
        return faultController.ConditionState() == state_machine::FaultConditionState::asserted;
    }

    ClearRefusal OperationFlow::EvaluateClearFault()
    {
        faultController.SampleCondition();
        return faultController.EvaluateClear();
    }

    bool OperationFlow::CanClearFault()
    {
        return EvaluateClearFault() == ClearRefusal::none;
    }

    void OperationFlow::TraceFaultClearRefused(ClearRefusal refusal) const
    {
        switch (refusal)
        {
            case ClearRefusal::conditionAsserted:
                env.tracer.Trace() << "[SM] Fault clear refused: board protection still asserted";
                break;
            case ClearRefusal::dwellNotElapsed:
                env.tracer.Trace() << "[SM] Fault clear refused: board protection has not been clear long enough";
                break;
            default:
                env.tracer.Trace() << "[SM] Fault clear refused, retry limit reached; reset required";
                break;
        }
    }

    state_machine::Ready OperationFlow::ClearFaultToReady()
    {
        really_assert(faultController.TryClear());
        env.tracer.Trace() << "[SM] Fault cleared";
        return calibration.ReadyState();
    }

    state_machine::Idle OperationFlow::ClearFaultToIdle()
    {
        really_assert(faultController.TryClear());
        env.tracer.Trace() << "[SM] Fault cleared";
        return state_machine::Idle{};
    }

    void OperationFlow::AbortActiveWork()
    {
        env.tracer.Trace() << "[SM] Emergency stop";
        env.mode.GetFocControl().Stop();
        calibration.Abort();
        offsetRefresh.Abort();
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

    bool OperationFlow::StartUnlessFaulted()
    {
        if (DispatchPendingFault())
            return false;

        env.mode.GetFocControl().Start();

        if (DispatchPendingFault() || faultDispatched)
            return false;

        faultController.StartCleanRun();
        return true;
    }

    bool OperationFlow::DispatchPendingFault()
    {
        const auto code = faultController.TakePendingFault();

        if (!code.has_value())
            return false;

        faultDispatched = true;
        env.machine.Dispatch(state_machine::FaultDetected{ *code });
        return true;
    }

    void OperationFlow::StateEntered(StateId state)
    {
        // A fault raised by the start owns the pending result: the transition it queued completes it with abortedByFault.
        if (state.Is<state_machine::Enabled>())
        {
            if (!StartUnlessFaulted())
                return;
        }
        else
            faultController.StopCleanRun();

        env.pending.FlushDeferred();

        if (state.Is<state_machine::Ready>() && readyHandler != nullptr)
            readyHandler();
    }
}
