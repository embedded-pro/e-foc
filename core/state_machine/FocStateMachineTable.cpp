#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    namespace
    {
        bool WasActive(const state_machine::State& state)
        {
            return std::holds_alternative<state_machine::Enabled>(state) || std::holds_alternative<state_machine::Calibrating>(state);
        }
    }

    template<class Stopped>
    constexpr std::array<FocStateMachineCommon::Transition, 3> FocStateMachineCommon::CalibrationEntryRows()
    {
        return {
            StateMachine::Row<Stopped, state_machine::Calibrate, state_machine::Calibrating>(
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::Calibrate&)
                {
                    return !owner.HasPendingCommand();
                },
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::Calibrate& command)
                {
                    return owner.BeginCalibration(command);
                }),
            StateMachine::Row<Stopped, state_machine::ReAlign, state_machine::Calibrating>(
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::ReAlign&)
                {
                    return !owner.HasPendingCommand() && owner.HasValidCalibration();
                },
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::ReAlign& command)
                {
                    return owner.BeginReAlign(command);
                }),
            StateMachine::Row<Stopped, state_machine::ReserveExternalCalibration, state_machine::Calibrating>(
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::ReserveExternalCalibration&)
                {
                    return !owner.HasPendingAsyncWork();
                },
                [](FocStateMachineCommon&, Stopped&, const state_machine::ReserveExternalCalibration&)
                {
                    return state_machine::Calibrating{ state_machine::CalibrationStep::polePairs, {}, true };
                }),
        };
    }

    constexpr std::array<FocStateMachineCommon::Transition, 12> FocStateMachineCommon::CalibrationRows()
    {
        return services::JoinRows(CalibrationEntryRows<state_machine::Idle>(), CalibrationEntryRows<state_machine::Ready>(),
            std::array{
                StateMachine::InternalRow<state_machine::Calibrating, state_machine::CompleteExternalCalibration>(
                    [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command)
                    {
                        owner.BeginExternalCalibration(calibrating, command);
                    },
                    [](FocStateMachineCommon& owner, const state_machine::Calibrating&, const state_machine::CompleteExternalCalibration& command)
                    {
                        return owner.IsPlausibleExternalCalibration(command);
                    }),
                StateMachine::InternalRow<state_machine::Calibrating, state_machine::RunCalibrationSequence>(
                    [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::RunCalibrationSequence&)
                    {
                        owner.StartCalibrationSequence(calibrating);
                    }),
                StateMachine::InternalRow<state_machine::Calibrating, state_machine::RunAlignmentOnly>(
                    [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::RunAlignmentOnly&)
                    {
                        owner.StartAlignmentOnly(calibrating);
                    }),
                StateMachine::InternalRow<state_machine::Calibrating, state_machine::CalibrationStepChanged>(
                    [](FocStateMachineCommon&, state_machine::Calibrating& calibrating, const state_machine::CalibrationStepChanged& event)
                    {
                        calibrating.step = event.step;
                    }),
                StateMachine::InternalRow<state_machine::Calibrating, state_machine::AlignmentSucceeded>(
                    [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::AlignmentSucceeded& event)
                    {
                        owner.OnAlignmentSucceeded(calibrating, event.angle);
                    }),
                StateMachine::InternalRow<state_machine::Calibrating, state_machine::MechanicalParametersIdentified>(
                    [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
                    {
                        owner.OnMechanicalParametersIdentified(calibrating, event);
                    },
                    [](FocStateMachineCommon&, const state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified&)
                    {
                        return calibrating.step == state_machine::CalibrationStep::frictionAndInertia;
                    }),
            });
    }

    constexpr std::array<FocStateMachineCommon::Transition, 4> FocStateMachineCommon::CalibrationCompletionRows()
    {
        return {
            StateMachine::Row<state_machine::Calibrating, state_machine::CalibrationStepFailed, state_machine::Fault>(nullptr,
                [](FocStateMachineCommon& owner, state_machine::Calibrating&, const state_machine::CalibrationStepFailed&)
                {
                    return owner.BuildFault(state_machine::FaultCode::calibrationFailed, true, state_machine::CommandResult::calibrationFailed);
                }),
            StateMachine::Row<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Fault>(
                [](FocStateMachineCommon&, const state_machine::Calibrating&, const state_machine::CalibrationSaved& event)
                {
                    return event.status != services::NvmStatus::Ok;
                },
                [](FocStateMachineCommon& owner, state_machine::Calibrating&, const state_machine::CalibrationSaved&)
                {
                    return owner.BuildFault(state_machine::FaultCode::calibrationFailed, true, state_machine::CommandResult::nvmFailed);
                }),
            StateMachine::Row<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Ready>(
                [](FocStateMachineCommon&, const state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
                {
                    return calibrating.pendingData.stage == services::CalibrationStage::complete;
                },
                [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
                {
                    return owner.CompleteCalibration(calibrating);
                }),
            StateMachine::Row<state_machine::Calibrating, state_machine::CalibrationSaved, state_machine::Idle>(nullptr,
                [](FocStateMachineCommon& owner, state_machine::Calibrating& calibrating, const state_machine::CalibrationSaved&)
                {
                    return owner.CompletePartialCalibration(calibrating);
                }),
        };
    }

    constexpr std::array<FocStateMachineCommon::Transition, 2> FocStateMachineCommon::OperationRows()
    {
        return {
            StateMachine::Row<state_machine::Ready, state_machine::Enable, state_machine::Enabled>(
                [](FocStateMachineCommon& owner, const state_machine::Ready& ready, const state_machine::Enable&)
                {
                    return owner.IsEnableAllowed(ready);
                },
                [](FocStateMachineCommon& owner, state_machine::Ready&, const state_machine::Enable&)
                {
                    return owner.BuildEnabled();
                }),
            StateMachine::Row<state_machine::Enabled, state_machine::Disable, state_machine::Ready>(nullptr,
                [](FocStateMachineCommon& owner, state_machine::Enabled&, const state_machine::Disable&)
                {
                    return owner.DisableToReady();
                }),
        };
    }

    template<class Active>
    constexpr std::array<FocStateMachineCommon::Transition, 2> FocStateMachineCommon::EmergencyStopRows()
    {
        return {
            StateMachine::Row<Active, state_machine::EmergencyStop, state_machine::Ready>(
                [](FocStateMachineCommon& owner, const Active&, const state_machine::EmergencyStop&)
                {
                    return owner.HasValidCalibration();
                },
                [](FocStateMachineCommon& owner, Active&, const state_machine::EmergencyStop&)
                {
                    return owner.StopToReady();
                }),
            StateMachine::Row<Active, state_machine::EmergencyStop, state_machine::Idle>(nullptr,
                [](FocStateMachineCommon& owner, Active&, const state_machine::EmergencyStop&)
                {
                    return owner.StopToIdle();
                }),
        };
    }

    template<class S>
    constexpr FocStateMachineCommon::Transition FocStateMachineCommon::EmergencyStopInternalRow()
    {
        return StateMachine::InternalRow<S, state_machine::EmergencyStop>([](FocStateMachineCommon& owner, S&, const state_machine::EmergencyStop&)
            {
                owner.StopWithoutTransition();
            });
    }

    constexpr std::array<FocStateMachineCommon::Transition, 10> FocStateMachineCommon::SafetyRows()
    {
        return services::JoinRows(
            std::array{
                StateMachine::RowFromAny<state_machine::FaultDetected, state_machine::Fault>(nullptr,
                    [](FocStateMachineCommon& owner, state_machine::State& from, const state_machine::FaultDetected& event)
                    {
                        return owner.BuildFault(event.code, WasActive(from));
                    }),
                StateMachine::Row<state_machine::Fault, state_machine::ClearFault, state_machine::Ready>(
                    [](FocStateMachineCommon& owner, const state_machine::Fault&, const state_machine::ClearFault&)
                    {
                        return owner.HasValidCalibration();
                    },
                    [](FocStateMachineCommon& owner, state_machine::Fault&, const state_machine::ClearFault&)
                    {
                        return owner.ClearFaultToReady();
                    }),
                StateMachine::Row<state_machine::Fault, state_machine::ClearFault, state_machine::Idle>(nullptr,
                    [](FocStateMachineCommon& owner, state_machine::Fault&, const state_machine::ClearFault&)
                    {
                        return owner.ClearFaultToIdle();
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
    constexpr std::array<FocStateMachineCommon::Transition, 5> FocStateMachineCommon::MaintenanceRowsFor()
    {
        return {
            StateMachine::InternalRow<Stopped, state_machine::ClearCalibration>(
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::ClearCalibration& command)
                {
                    owner.BeginClearCalibration(command);
                },
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::ClearCalibration&)
                {
                    return !owner.HasPendingCommand();
                }),
            StateMachine::Row<Stopped, state_machine::CalibrationInvalidated, state_machine::Idle>(
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::CalibrationInvalidated& event)
                {
                    return owner.HasPendingCommand() && event.status == services::NvmStatus::Ok;
                },
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::CalibrationInvalidated&)
                {
                    return owner.CompleteClearCalibration();
                }),
            StateMachine::Row<Stopped, state_machine::CalibrationInvalidated, state_machine::Fault>(
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::CalibrationInvalidated& event)
                {
                    return owner.HasPendingCommand() && event.status != services::NvmStatus::Busy;
                },
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::CalibrationInvalidated&)
                {
                    return owner.BuildFault(state_machine::FaultCode::hardwareFault, false, state_machine::CommandResult::nvmFailed);
                }),
            StateMachine::InternalRow<Stopped, state_machine::CalibrationInvalidated>(
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::CalibrationInvalidated&)
                {
                    owner.CompletePendingCommand(state_machine::CommandResult::rejected);
                }),
            StateMachine::InternalRow<Stopped, state_machine::SetFluxLinkage>(
                [](FocStateMachineCommon& owner, Stopped&, const state_machine::SetFluxLinkage& command)
                {
                    owner.BeginSetFluxLinkage(command);
                },
                [](FocStateMachineCommon& owner, const Stopped&, const state_machine::SetFluxLinkage& command)
                {
                    return owner.IsAcceptableFluxLinkage(command);
                }),
        };
    }

    template<class Stopped>
    constexpr FocStateMachineCommon::Transition FocStateMachineCommon::FluxLinkageSavedRow()
    {
        return StateMachine::InternalRow<Stopped, state_machine::FluxLinkageSaved>(
            [](FocStateMachineCommon& owner, Stopped&, const state_machine::FluxLinkageSaved& event)
            {
                owner.OnFluxLinkageSaved(event.status);
            },
            [](FocStateMachineCommon& owner, const Stopped&, const state_machine::FluxLinkageSaved&)
            {
                return owner.HasPendingCommand();
            });
    }

    constexpr std::array<FocStateMachineCommon::Transition, 12> FocStateMachineCommon::MaintenanceRows()
    {
        return services::JoinRows(MaintenanceRowsFor<state_machine::Idle>(), MaintenanceRowsFor<state_machine::Ready>(),
            std::array{
                FluxLinkageSavedRow<state_machine::Idle>(),
                FluxLinkageSavedRow<state_machine::Ready>(),
            });
    }

    constexpr std::array<FocStateMachineCommon::Transition, 2> FocStateMachineCommon::BootRows()
    {
        return {
            StateMachine::InternalRow<state_machine::Idle, state_machine::BootValidityChecked>(
                [](FocStateMachineCommon& owner, state_machine::Idle&, const state_machine::BootValidityChecked& event)
                {
                    owner.ContinueBoot(event.valid);
                }),
            StateMachine::InternalRow<state_machine::Idle, state_machine::BootCalibrationLoaded>(
                [](FocStateMachineCommon& owner, state_machine::Idle&, const state_machine::BootCalibrationLoaded& event)
                {
                    owner.FinishBoot(event.status);
                }),
        };
    }

    FocStateMachineCommon::StateMachine::Table FocStateMachineCommon::Rows()
    {
        static constexpr auto rows = services::JoinRows(CalibrationRows(), CalibrationCompletionRows(), OperationRows(), SafetyRows(), MaintenanceRows(), BootRows());
        return infra::MakeRange(rows);
    }

    void FocStateMachineCommon::RegisterEnteredHooks()
    {
        stateMachine.OnEntered<state_machine::Idle>([](FocStateMachineCommon& owner, state_machine::Idle&)
            {
                owner.OnStateEntered(StateId::Of<state_machine::Idle>());
            });
        stateMachine.OnEntered<state_machine::Calibrating>([](FocStateMachineCommon& owner, state_machine::Calibrating&)
            {
                owner.OnStateEntered(StateId::Of<state_machine::Calibrating>());
            });
        stateMachine.OnEntered<state_machine::Ready>([](FocStateMachineCommon& owner, state_machine::Ready&)
            {
                owner.OnStateEntered(StateId::Of<state_machine::Ready>());
            });
        stateMachine.OnEntered<state_machine::Enabled>([](FocStateMachineCommon& owner, state_machine::Enabled&)
            {
                owner.OnStateEntered(StateId::Of<state_machine::Enabled>());
            });
        stateMachine.OnEntered<state_machine::Fault>([](FocStateMachineCommon& owner, state_machine::Fault&)
            {
                owner.OnStateEntered(StateId::Of<state_machine::Fault>());
            });
    }
}
