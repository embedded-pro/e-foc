#include "core/state_machine/OperationFlow.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/Math.hpp"

namespace application
{
    OperationFlow::OperationFlow(const LifecycleEnvironment& environment, CalibrationFlow& calibration, const CalibrationServices& services)
        : env(environment)
        , calibration(calibration)
        , offsetRefresh(services.currentOffsets, services.standstill, environment.tracer)
        , index(services.indexSearch, environment.tracer)
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

    void OperationFlow::RequireHoming(bool required)
    {
        homingRequired = required;
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

        const bool referenced = ready.rotorReferenceValid || env.context.IsRotorReferenceValid() || IsIndexSearchDue();

        if (!referenced)
        {
            env.tracer.Trace() << "[SM] Enable rejected: rotor reference not established; run alignment";
            return false;
        }

        if (homingRequired && !env.context.IsHomed() && !IsIndexSearchDue())
        {
            env.tracer.Trace() << "[SM] Enable rejected: position mode requires homing; calibrate or restore the reference from the encoder index";
            return false;
        }

        return true;
    }

    bool OperationFlow::IsEnablePreparationDue() const
    {
        return offsetRefresh.IsDue() || IsIndexSearchDue();
    }

    bool OperationFlow::IsIndexSearchDue() const
    {
        return !env.context.IsRotorReferenceValid() && index.CanRestore(env.context.Data());
    }

    bool OperationFlow::IsPreparingEnable() const
    {
        return offsetRefresh.IsRunning() || index.IsRunning();
    }

    const IndexReference& OperationFlow::Index() const
    {
        return index;
    }

    void OperationFlow::BeginEnablePreparation(const state_machine::Ready& ready, const state_machine::Enable& command)
    {
        env.pending.Accept(command.onDone);

        if (!offsetRefresh.IsDue())
        {
            SearchIndex();
            return;
        }

        const auto& record = ready.loadedData;
        offsetRefresh.Start(foc::PhaseCurrents{ foc::Ampere{ record.currentOffsetA }, foc::Ampere{ record.currentOffsetB }, foc::Ampere{ record.currentOffsetC } },
            [this](state_machine::OffsetRefreshOutcome outcome)
            {
                env.machine.Dispatch(state_machine::OffsetsRefreshed{ outcome });
            });
    }

    void OperationFlow::SearchIndex()
    {
        index.Start(env.context.Data(), env.context.BusVoltage(), [this](bool found)
            {
                if (found)
                {
                    env.context.SetRotorReferenceValid(true);
                    env.context.SetHomed(true);
                }

                env.machine.Dispatch(state_machine::IndexSearched{ found });
            });
    }

    void OperationFlow::RefuseAfterOffsetRefresh(state_machine::OffsetRefreshOutcome outcome)
    {
        env.pending.Complete(outcome == state_machine::OffsetRefreshOutcome::refreshed ? state_machine::CommandResult::rejected : state_machine::CommandResult::calibrationFailed);
    }

    void OperationFlow::RefuseAfterIndexSearch()
    {
        env.pending.Complete(state_machine::CommandResult::calibrationFailed);
    }

    void OperationFlow::AbandonEnablePreparation()
    {
        env.tracer.Trace() << "[SM] Enable abandoned: disabled while preparing to enable";
        AbortEnablePreparation();
        env.pending.Complete(state_machine::CommandResult::rejected);
    }

    void OperationFlow::AbortEnablePreparation()
    {
        offsetRefresh.Abort();
        index.Abort();
    }

    state_machine::Enabled OperationFlow::BuildEnabled(const state_machine::Enable& command)
    {
        env.pending.Accept(command.onDone);
        return BuildEnabledAfterPreparation();
    }

    state_machine::Enabled OperationFlow::BuildEnabledAfterPreparation()
    {
        env.mode.PrepareForEnabled();
        env.pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Enabled{};
    }

    state_machine::Ready OperationFlow::DisableToReady()
    {
        quickStopTimer.Cancel();
        env.mode.GetFocControl().Stop();
        faultController.ResetClearCount();
        return calibration.ReadyState();
    }

    bool OperationFlow::CanBeginQuickStop() const
    {
        return env.mode.CanBeginQuickStop();
    }

    state_machine::Stopping OperationFlow::BeginQuickStop()
    {
        env.tracer.Trace() << "[SM] Quick stop";
        quickStopStarted = infra::Now();

        const auto initialSpeed = math::Abs(env.mode.GetFoc().ObserveMotion().measuredSpeed.Value());
        const auto deceleration = env.mode.QuickStopDeceleration().Value();
        dynamicStopTimeout = deceleration > 0.0f
                                 ? std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(initialSpeed / deceleration * 1.25f + 0.1f))
                                 : quickStopTimeout;

        if (env.mode.BeginQuickStop())
            quickStopTimer.Start(quickStopCheckPeriod, [this]()
                {
                    CheckQuickStop();
                });
        else
        {
            env.tracer.Trace() << "[SM] Quick stop: this mode cannot brake without a calibrated inertia; power stage cut";
            env.mode.GetFocControl().Stop();
            env.machine.Dispatch(state_machine::StopCompleted{});
        }

        return state_machine::Stopping{};
    }

    void OperationFlow::CheckQuickStop()
    {
        const auto speed = math::Abs(env.mode.GetFoc().ObserveMotion().measuredSpeed.Value());
        if (speed >= quickStopSpeed && infra::Now() - quickStopStarted < dynamicStopTimeout)
            return;

        quickStopTimer.Cancel();
        env.machine.Dispatch(state_machine::StopCompleted{});
    }

    state_machine::Ready OperationFlow::CompleteQuickStop()
    {
        env.tracer.Trace() << "[SM] Quick stop complete";
        return DisableToReady();
    }

    state_machine::Fault OperationFlow::EnterFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult)
    {
        env.tracer.Trace() << "[SM] Fault: " << state_machine::FaultCodeName(code);

        const bool newEpisode = !faultController.IsRecorded();
        if (newEpisode)
            lastFaultCode = code;
        else
            env.tracer.Trace() << "[SM] Further fault while faulted; keeping the first code";

        faultDispatched = false;
        quickStopTimer.Cancel();
        faultController.EnterFault();
        faultController.SampleCondition();
        AbortEnablePreparation();
        offsetRefresh.MarkStale();

        if (wasActive)
            env.mode.GetFocControl().Stop();

        calibration.Abort();
        env.pending.CompleteAfterTransition(pendingResult);

        if (newEpisode && faultEnteredHandler != nullptr)
            faultEnteredHandler(code);

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
        quickStopTimer.Cancel();
        env.mode.GetFocControl().Stop();
        calibration.Abort();
        AbortEnablePreparation();
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

    void OperationFlow::RegisterFaultEnteredHandler(const infra::Function<void(state_machine::FaultCode)>& onFaultEntered)
    {
        faultEnteredHandler = onFaultEntered;
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
