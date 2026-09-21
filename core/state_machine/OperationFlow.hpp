#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/FaultController.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "infra/util/Function.hpp"

namespace application
{
    class OperationFlow
    {
    public:
        using StateId = LifecycleMachine::StateId;

        OperationFlow(const LifecycleEnvironment& environment, CalibrationFlow& calibration);

        void RegisterFaultHandler(state_machine::FaultNotifier& notifier);
        void UnregisterFaultHandler();
        state_machine::FaultCode LastFaultCode() const;
        bool HasPendingFault() const;

        bool IsEnableAllowed(const state_machine::Ready& ready);
        state_machine::Enabled BuildEnabled();
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
        void StateEntered(StateId state);

    private:
        void AbortActiveWork();
        void StartUnlessFaulted();
        bool DispatchPendingFault();

    private:
        const LifecycleEnvironment& env;
        CalibrationFlow& calibration;
        FaultController faultController;
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::none };
        infra::Function<void()> readyHandler;
    };
}
