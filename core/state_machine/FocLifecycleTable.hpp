#pragma once

#include "core/state_machine/LifecycleContext.hpp"
#include "services/fsm/ValidatedTable.hpp"
#include <array>

namespace application
{
    class FocLifecycleTable
    {
    public:
        using Machine = LifecycleMachine;
        using Initial = state_machine::Idle;

        static services::ValidatedTable<LifecycleMachine> Table();
        static constexpr std::array<LifecycleMachine::Transition, 45> Rows();
        static constexpr LifecycleMachine::Rules Rules();
        static void RegisterEnteredHooks(LifecycleMachine& machine);

    private:
        using Transition = LifecycleMachine::Transition;

        static constexpr std::array<Transition, 12> CalibrationRows();
        static constexpr std::array<Transition, 4> CalibrationCompletionRows();
        static constexpr std::array<Transition, 2> OperationRows();
        static constexpr std::array<Transition, 10> SafetyRows();
        static constexpr std::array<Transition, 15> MaintenanceRows();
        static constexpr std::array<Transition, 3> FaultReconciliationRows();
        static constexpr std::array<Transition, 2> BootRows();

        template<class Stopped>
        static constexpr std::array<Transition, 3> CalibrationEntryRows();
        template<class Active>
        static constexpr std::array<Transition, 2> EmergencyStopRows();
        template<class S>
        static constexpr Transition EmergencyStopInternalRow();
        template<class Stopped>
        static constexpr std::array<Transition, 5> MaintenanceRowsFor();
        template<class S>
        static constexpr Transition FluxLinkageSavedRow();
        template<class S>
        static void RegisterEnteredHook(LifecycleMachine& machine);
    };
}
