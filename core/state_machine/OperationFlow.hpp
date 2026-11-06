#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/CurrentOffsetRefresh.hpp"
#include "core/state_machine/FaultController.hpp"
#include "core/state_machine/IndexReference.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "infra/util/Function.hpp"

namespace application
{
    class OperationFlow
    {
    public:
        using StateId = LifecycleMachine::StateId;

        OperationFlow(const LifecycleEnvironment& environment, CalibrationFlow& calibration, const CalibrationServices& services);

        void RegisterFaultHandler(state_machine::FaultNotifier& notifier);
        void UnregisterFaultHandler();
        state_machine::FaultCode LastFaultCode() const;
        bool HasPendingFault() const;

        bool IsEnableAllowed(const state_machine::Ready& ready);
        bool IsEnablePreparationDue() const;
        bool IsIndexSearchDue() const;
        bool IsPreparingEnable() const;
        const IndexReference& Index() const;
        void BeginEnablePreparation(const state_machine::Ready& ready, const state_machine::Enable& command);
        void SearchIndex();
        void RefuseAfterOffsetRefresh(state_machine::OffsetRefreshOutcome outcome);
        void RefuseAfterIndexSearch();
        void AbandonEnablePreparation();
        void AbortEnablePreparation();
        state_machine::Enabled BuildEnabled(const state_machine::Enable& command);
        state_machine::Enabled BuildEnabledAfterPreparation();
        state_machine::Ready DisableToReady();

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
        void RegisterFaultEnteredHandler(const infra::Function<void(state_machine::FaultCode)>& onFaultEntered);
        void StateEntered(StateId state);

    private:
        void AbortActiveWork();
        bool StartUnlessFaulted();
        bool DispatchPendingFault();

    private:
        const LifecycleEnvironment& env;
        CalibrationFlow& calibration;
        FaultController faultController;
        CurrentOffsetRefresh offsetRefresh;
        IndexReference index;
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::none };
        bool faultDispatched{ false };
        infra::Function<void()> readyHandler;
        infra::Function<void(state_machine::FaultCode)> faultEnteredHandler;
    };
}
