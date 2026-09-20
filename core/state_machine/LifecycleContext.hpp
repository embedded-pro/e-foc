#pragma once

#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "services/fsm/TableStateMachine.hpp"

namespace application
{
    class BootSequence;
    class CalibrationFlow;
    class MaintenanceFlow;
    class OperationFlow;
    class PendingCommand;

    struct LifecycleContext
    {
        const state_machine::FocStateMachineBase& lifecycle;
        CalibrationFlow& calibration;
        MaintenanceFlow& maintenance;
        BootSequence& boot;
        OperationFlow& operation;
        const PendingCommand& pending;
    };

    using LifecycleMachine = services::TableStateMachine<state_machine::State, state_machine::Event, LifecycleContext>;
}
