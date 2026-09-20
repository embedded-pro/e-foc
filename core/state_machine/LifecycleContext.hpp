#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "core/state_machine/ModeHooks.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/PendingCommand.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    class BootSequence;
    class CalibrationFlow;
    class MaintenanceFlow;
    class OperationFlow;

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

    struct LifecycleEnvironment
    {
        LifecycleMachine& machine;
        CalibrationContext& context;
        services::NonVolatileMemory& nvm;
        NvmActivity& nvmActivity;
        PendingCommand& pending;
        ModeHooks& mode;
        services::Tracer& tracer;
    };
}
