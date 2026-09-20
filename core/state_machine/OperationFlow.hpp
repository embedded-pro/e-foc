#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/FaultController.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "core/state_machine/ModeHooks.hpp"
#include "core/state_machine/PendingCommand.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    class OperationFlow
    {
    public:
        using StateId = LifecycleMachine::StateId;

        OperationFlow(LifecycleMachine& machine, CalibrationFlow& calibration, PendingCommand& pending, ModeHooks& mode, services::Tracer& tracer);

        void RegisterFaultHandler(state_machine::FaultNotifier& notifier);
        void UnregisterFaultHandler();
        state_machine::FaultCode LastFaultCode() const;

        bool IsEnableAllowed(const state_machine::Ready& ready) const;
        state_machine::Enabled BuildEnabled();
        state_machine::Ready DisableToReady();

        state_machine::Fault EnterFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult = state_machine::CommandResult::abortedByFault);
        bool TryClearFault();
        state_machine::Ready ClearFaultToReady();
        state_machine::Idle ClearFaultToIdle();

        void StopWithoutTransition();
        state_machine::Idle StopToIdle();
        state_machine::Ready StopToReady();

        void RegisterReadyHandler(const infra::Function<void()>& onReady);
        void StateEntered(StateId state);

    private:
        void AbortActiveWork();

    private:
        LifecycleMachine& machine;
        CalibrationFlow& calibration;
        PendingCommand& pending;
        ModeHooks& mode;
        services::Tracer& tracer;
        FaultController faultController;
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::none };
        infra::Function<void()> readyHandler;
    };
}
