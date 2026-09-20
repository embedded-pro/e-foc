#include "core/state_machine/FocLifecycleTable.hpp"
#include "core/state_machine/BootSequence.hpp"
#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/MaintenanceFlow.hpp"
#include "core/state_machine/OperationFlow.hpp"
#include "core/state_machine/PendingCommand.hpp"

namespace application
{
    namespace
    {
        using Machine = LifecycleMachine;

        bool WasActive(const state_machine::State& state)
        {
            return std::holds_alternative<state_machine::Enabled>(state) || std::holds_alternative<state_machine::Calibrating>(state);
        }
    }

    template<class Stopped>
    constexpr std::array<FocLifecycleTable::Transition, 3> FocLifecycleTable::CalibrationEntryRows()
    {
        return {
            Machine::Row<Stopped, state_machine::Calibrate, state_machine::Calibrating>(
                [](const LifecycleContext& context, const Stopped&, const state_machine::Calibrate&)
                {
                    return !context.lifecycle.HasPendingAsyncWork();
                },
                [](LifecycleContext& context, Stopped&, const state_machine::Calibrate& command)
                {
                    return context.calibration.Begin(command);
                }),
            Machine::Row<Stopped, state_machine::ReAlign, state_machine::Calibrating>(
                [](const LifecycleContext& context, const Stopped&, const state_machine::ReAlign&)
                {
                    return !context.lifecycle.HasPendingAsyncWork() && context.calibration.HasValidCalibration();
                },
                [](LifecycleContext& context, Stopped&, const state_machine::ReAlign& command)
                {
                    return context.calibration.BeginReAlign(command);
                }),
            Machine::Row<Stopped, state_machine::ReserveExternalCalibration, state_machine::Calibrating>(
                [](const LifecycleContext& context, const Stopped&, const state_machine::ReserveExternalCalibration&)
                {
                    return !context.lifecycle.HasPendingAsyncWork();
                },
                [](LifecycleContext&, Stopped&, const state_machine::ReserveExternalCalibration&)
                {
                    return CalibrationFlow::Reserve();
                }),
        };
    }

    constexpr std::array<FocLifecycleTable::Transition, 12> FocLifecycleTable::CalibrationRows()
    {
        return services::JoinRows(CalibrationEntryRows<state_machine::Idle>(), CalibrationEntryRows<state_machine::Ready>(),
            std::array{
                Machine::InternalRow<state_machine::Calibrating, state_machine::CompleteExternalCalibration>(
                    [](LifecycleContext& context, state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command)
                    {
                        context.calibration.BeginExternal(calibrating, command);
                    },
                    [](const LifecycleContext& context, const state_machine::Calibrating&, const state_machine::CompleteExternalCalibration& command)
                    {
                        return !context.pending.Pending() && !context.calibration.IsRunning() && context.calibration.IsPlausibleExternal(command);
                    }),
                Machine::InternalRow<state_machine::Calibrating, state_machine::RunCalibrationSequence>(
                    [](LifecycleContext& context, state_machine::Calibrating& calibrating, const state_machine::RunCalibrationSequence&)
                    {
                        context.calibration.RunSequence(calibrating);
                    }),
                Machine::InternalRow<state_machine::Calibrating, state_machine::RunAlignmentOnly>(
                    [](LifecycleContext& context, state_machine::Calibrating& calibrating, const state_machine::RunAlignmentOnly&)
                    {
                        context.calibration.RunAlignmentOnly(calibrating);
                    }),
                Machine::InternalRow<state_machine::Calibrating, state_machine::CalibrationStepChanged>(
                    [](LifecycleContext&, state_machine::Calibrating& calibrating, const state_machine::CalibrationStepChanged& event)
                    {
                        calibrating.step = event.step;
                    }),
                Machine::InternalRow<state_machine::Calibrating, state_machine::AlignmentSucceeded>(
                    [](LifecycleContext& context, state_machine::Calibrating& calibrating, const state_machine::AlignmentSucceeded& event)
                    {
                        context.calibration.OnAlignmentSucceeded(calibrating, event.angle);
                    }),
                Machine::InternalRow<state_machine::Calibrating, state_machine::MechanicalParametersIdentified>(
                    [](LifecycleContext& context, state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
                    {
                        context.calibration.OnMechanicalParametersIdentified(calibrating, event);
                    },
                    [](const LifecycleContext&, const state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified&)
                    {
                        return calibrating.step == state_machine::CalibrationStep::frictionAndInertia;
                    }),
            });
    }

    constexpr std::array<FocLifecycleTable::Transition, 4> FocLifecycleTable::CalibrationCompletionRows()
    {
        return {
            Machine::Row<state_machine::Calibrating, state_machine::CalibrationStepFailed, state_machine::Fault>(nullptr,
                [](LifecycleContext& context, state_machine::Calibrating&, const state_machine::CalibrationStepFailed&)
                {
                    return context.operation.EnterFault(state_machine::FaultCode::calibrationFailed, true, state_machine::CommandResult::calibrationFailed);
                }),
            Machine::Row<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Fault>(
                [](const LifecycleContext&, const state_machine::Calibrating&, const state_machine::CalibrationSaved& event)
                {
                    return event.status != services::NvmStatus::Ok;
                },
                [](LifecycleContext& context, state_machine::Calibrating&, const state_machine::CalibrationSaved&)
                {
                    return context.operation.EnterFault(state_machine::FaultCode::calibrationFailed, true, state_machine::CommandResult::nvmFailed);
                }),
            Machine::Row<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Ready>(
                [](const LifecycleContext&, const state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved& event)
                {
                    return event.status == services::NvmStatus::Ok && calibrating.pendingData.stage == services::CalibrationStage::complete;
                },
                [](LifecycleContext& context, const state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
                {
                    return context.calibration.Complete(calibrating);
                }),
            Machine::Row<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Idle>(nullptr,
                [](LifecycleContext& context, const state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
                {
                    return context.calibration.CompletePartial(calibrating);
                }),
        };
    }

    constexpr std::array<FocLifecycleTable::Transition, 2> FocLifecycleTable::OperationRows()
    {
        return {
            Machine::Row<state_machine::Ready, state_machine::Enable, state_machine::Enabled>(
                [](const LifecycleContext& context, const state_machine::Ready& ready, const state_machine::Enable&)
                {
                    return !context.lifecycle.HasPendingAsyncWork() && context.operation.IsEnableAllowed(ready);
                },
                [](LifecycleContext& context, state_machine::Ready&, const state_machine::Enable&)
                {
                    return context.operation.BuildEnabled();
                }),
            Machine::Row<state_machine::Enabled, state_machine::Disable, state_machine::Ready>(nullptr,
                [](LifecycleContext& context, state_machine::Enabled&, const state_machine::Disable&)
                {
                    return context.operation.DisableToReady();
                }),
        };
    }

    template<class Active>
    constexpr std::array<FocLifecycleTable::Transition, 2> FocLifecycleTable::EmergencyStopRows()
    {
        return {
            Machine::Row<Active, state_machine::EmergencyStop, state_machine::Ready>(
                [](const LifecycleContext& context, const Active&, const state_machine::EmergencyStop&)
                {
                    return context.calibration.HasValidCalibration();
                },
                [](LifecycleContext& context, Active&, const state_machine::EmergencyStop&)
                {
                    return context.operation.StopToReady();
                }),
            Machine::Row<Active, state_machine::EmergencyStop, state_machine::Idle>(nullptr,
                [](LifecycleContext& context, Active&, const state_machine::EmergencyStop&)
                {
                    return context.operation.StopToIdle();
                }),
        };
    }

    template<class S>
    constexpr FocLifecycleTable::Transition FocLifecycleTable::EmergencyStopInternalRow()
    {
        return Machine::InternalRow<S, state_machine::EmergencyStop>([](LifecycleContext& context, S&, const state_machine::EmergencyStop&)
            {
                context.operation.StopWithoutTransition();
            });
    }

    constexpr std::array<FocLifecycleTable::Transition, 10> FocLifecycleTable::SafetyRows()
    {
        return services::JoinRows(
            std::array{
                Machine::RowFromAny<state_machine::FaultDetected, state_machine::Fault>(nullptr,
                    [](LifecycleContext& context, const state_machine::State& from, const state_machine::FaultDetected& event)
                    {
                        return context.operation.EnterFault(event.code, WasActive(from));
                    }),
                Machine::Row<state_machine::Fault, state_machine::ClearFault, state_machine::Ready>(
                    [](const LifecycleContext& context, const state_machine::Fault&, const state_machine::ClearFault&)
                    {
                        return context.calibration.HasValidCalibration();
                    },
                    [](LifecycleContext& context, state_machine::Fault&, const state_machine::ClearFault&)
                    {
                        return context.operation.ClearFaultToReady();
                    }),
                Machine::Row<state_machine::Fault, state_machine::ClearFault, state_machine::Idle>(nullptr,
                    [](LifecycleContext& context, state_machine::Fault&, const state_machine::ClearFault&)
                    {
                        return context.operation.ClearFaultToIdle();
                    }),
            },
            EmergencyStopRows<state_machine::Enabled>(),
            EmergencyStopRows<state_machine::Calibrating>(),
            std::array{
                EmergencyStopInternalRow<state_machine::Idle>(),
                EmergencyStopInternalRow<state_machine::Ready>(),
                EmergencyStopInternalRow<state_machine::Fault>(),
            });
    }

    template<class Stopped>
    constexpr std::array<FocLifecycleTable::Transition, 5> FocLifecycleTable::MaintenanceRowsFor()
    {
        return {
            Machine::InternalRow<Stopped, state_machine::ClearCalibration>(
                [](LifecycleContext& context, Stopped&, const state_machine::ClearCalibration& command)
                {
                    context.maintenance.BeginClear(command);
                },
                [](const LifecycleContext& context, const Stopped&, const state_machine::ClearCalibration&)
                {
                    return !context.lifecycle.HasPendingAsyncWork();
                }),
            Machine::Row<Stopped, state_machine::CalibrationInvalidated, state_machine::Idle>(
                [](const LifecycleContext&, const Stopped&, const state_machine::CalibrationInvalidated& event)
                {
                    return event.status == services::NvmStatus::Ok;
                },
                [](LifecycleContext& context, Stopped&, const state_machine::CalibrationInvalidated&)
                {
                    return context.maintenance.CompleteClear();
                }),
            Machine::Row<Stopped, state_machine::CalibrationInvalidated, state_machine::Fault>(
                [](const LifecycleContext&, const Stopped&, const state_machine::CalibrationInvalidated& event)
                {
                    return event.status != services::NvmStatus::Ok && event.status != services::NvmStatus::Busy;
                },
                [](LifecycleContext& context, Stopped&, const state_machine::CalibrationInvalidated&)
                {
                    return context.operation.EnterFault(state_machine::FaultCode::hardwareFault, false, state_machine::CommandResult::nvmFailed);
                }),
            Machine::InternalRow<Stopped, state_machine::CalibrationInvalidated>(
                [](LifecycleContext& context, Stopped&, const state_machine::CalibrationInvalidated&)
                {
                    context.maintenance.RejectClear();
                }),
            Machine::InternalRow<Stopped, state_machine::SetFluxLinkage>(
                [](LifecycleContext& context, Stopped&, const state_machine::SetFluxLinkage& command)
                {
                    context.maintenance.BeginSetFluxLinkage(command);
                },
                [](const LifecycleContext& context, const Stopped&, const state_machine::SetFluxLinkage& command)
                {
                    return !context.lifecycle.HasPendingAsyncWork() && context.maintenance.IsAcceptableFluxLinkage(command);
                }),
        };
    }

    template<class S>
    constexpr FocLifecycleTable::Transition FocLifecycleTable::FluxLinkageSavedRow()
    {
        return Machine::InternalRow<S, state_machine::FluxLinkageSaved>(
            [](LifecycleContext& context, S&, const state_machine::FluxLinkageSaved& event)
            {
                context.maintenance.OnFluxLinkageSaved(event.status);
            });
    }

    constexpr std::array<FocLifecycleTable::Transition, 3> FocLifecycleTable::FaultReconciliationRows()
    {
        return {
            Machine::InternalRow<state_machine::Fault, state_machine::CalibrationInvalidated>(
                [](LifecycleContext& context, state_machine::Fault&, const state_machine::CalibrationInvalidated&)
                {
                    context.maintenance.ReconcileClear();
                },
                [](const LifecycleContext&, const state_machine::Fault&, const state_machine::CalibrationInvalidated& event)
                {
                    return event.status == services::NvmStatus::Ok;
                }),
            Machine::InternalRow<state_machine::Fault, state_machine::CalibrationInvalidated>(),
            FluxLinkageSavedRow<state_machine::Fault>(),
        };
    }

    constexpr std::array<FocLifecycleTable::Transition, 15> FocLifecycleTable::MaintenanceRows()
    {
        return services::JoinRows(MaintenanceRowsFor<state_machine::Idle>(), MaintenanceRowsFor<state_machine::Ready>(),
            std::array{
                FluxLinkageSavedRow<state_machine::Idle>(),
                FluxLinkageSavedRow<state_machine::Ready>(),
            },
            FaultReconciliationRows());
    }

    constexpr std::array<FocLifecycleTable::Transition, 2> FocLifecycleTable::BootRows()
    {
        return {
            Machine::InternalRow<state_machine::Idle, state_machine::BootValidityChecked>(
                [](LifecycleContext& context, state_machine::Idle&, const state_machine::BootValidityChecked& event)
                {
                    context.boot.Continue(event.valid);
                }),
            Machine::InternalRow<state_machine::Idle, state_machine::BootCalibrationLoaded>(
                [](LifecycleContext& context, state_machine::Idle&, const state_machine::BootCalibrationLoaded& event)
                {
                    context.boot.Finish(event.status);
                }),
        };
    }

    LifecycleMachine::Table FocLifecycleTable::Rows()
    {
        static constexpr auto rows = services::JoinRows(CalibrationRows(), CalibrationCompletionRows(), OperationRows(), SafetyRows(), MaintenanceRows(), BootRows());
        return infra::MakeRange(rows);
    }

    template<class S>
    void FocLifecycleTable::RegisterEnteredHook(LifecycleMachine& machine)
    {
        machine.OnEntered<S>([](LifecycleContext& context, S&)
            {
                context.operation.StateEntered(LifecycleMachine::StateId::Of<S>());
            });
    }

    void FocLifecycleTable::RegisterEnteredHooks(LifecycleMachine& machine)
    {
        RegisterEnteredHook<state_machine::Idle>(machine);
        RegisterEnteredHook<state_machine::Calibrating>(machine);
        RegisterEnteredHook<state_machine::Ready>(machine);
        RegisterEnteredHook<state_machine::Enabled>(machine);
        RegisterEnteredHook<state_machine::Fault>(machine);
    }
}
