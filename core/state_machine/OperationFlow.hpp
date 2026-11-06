#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/CurrentOffsetRefresh.hpp"
#include "core/state_machine/FaultController.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace application
{
    class OperationFlow
    {
    public:
        using StateId = LifecycleMachine::StateId;

        static constexpr infra::Duration quickStopCheckPeriod{ std::chrono::milliseconds(1) };
        static constexpr infra::Duration quickStopTimeout{ std::chrono::seconds(1) };
        static constexpr float quickStopSpeed{ 2.0f };

        OperationFlow(const LifecycleEnvironment& environment, CalibrationFlow& calibration, const CalibrationServices& services);

        void RegisterFaultHandler(state_machine::FaultNotifier& notifier);
        void UnregisterFaultHandler();
        state_machine::FaultCode LastFaultCode() const;
        bool HasPendingFault() const;

        bool IsEnableAllowed(const state_machine::Ready& ready);
        bool IsOffsetRefreshDue() const;
        bool IsRefreshingOffsets() const;
        void BeginOffsetRefresh(const state_machine::Ready& ready, const state_machine::Enable& command);
        void RefuseAfterOffsetRefresh(state_machine::OffsetRefreshOutcome outcome);
        void AbandonOffsetRefresh();
        void AbortOffsetRefresh();
        state_machine::Enabled BuildEnabled(const state_machine::Enable& command);
        state_machine::Enabled BuildEnabledAfterOffsetRefresh();
        state_machine::Ready DisableToReady();
        state_machine::Stopping BeginQuickStop();
        state_machine::Ready CompleteQuickStop();

        state_machine::Fault EnterFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult = state_machine::CommandResult::abortedByFault);
        bool IsFaultConditionAsserted() const;
        ClearRefusal EvaluateClearFault();
        bool CanClearFault();
        void TraceFaultClearRefused(ClearRefusal refusal) const;
        state_machine::Ready ClearFaultToReady();
        state_machine::Idle ClearFaultToIdle();

        void StopWithoutTransition();
        state_machine::Idle StopToIdle();
        state_machine::Ready StopToReady();

        void RegisterReadyHandler(const infra::Function<void()>& onReady);
        void StateEntered(StateId state);

    private:
        void AbortActiveWork();
        bool StartUnlessFaulted();
        bool DispatchPendingFault();
        void CheckQuickStop();

    private:
        const LifecycleEnvironment& env;
        CalibrationFlow& calibration;
        FaultController faultController;
        CurrentOffsetRefresh offsetRefresh;
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::none };
        bool faultDispatched{ false };
        infra::Function<void()> readyHandler;
        infra::TimerRepeating quickStopTimer;
        infra::TimePoint quickStopStarted;
    };
}
