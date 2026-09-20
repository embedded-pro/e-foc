#include "core/state_machine/FocStateMachineCommon.hpp"
#include <bit>

namespace application
{
    state_machine::Calibrating FocStateMachineCommon::BeginCalibration(const state_machine::Calibrate& command)
    {
        pendingCommandCallback = command.onDone;

        state_machine::Calibrating calibrating{};
        calibrating.pendingData.fluxLinkage = calibrationContext.EffectiveFluxLinkage().Value();

        Dispatch(state_machine::RunCalibrationSequence{});
        return calibrating;
    }

    state_machine::Calibrating FocStateMachineCommon::BeginReAlign(const state_machine::ReAlign& command)
    {
        pendingCommandCallback = command.onDone;
        calibrationContext.SetRotorReferenceValid(false);

        state_machine::Calibrating calibrating{};
        calibrating.pendingData = calibrationContext.Data();
        calibrating.alignmentOnly = true;

        Dispatch(state_machine::RunAlignmentOnly{});
        return calibrating;
    }

    void FocStateMachineCommon::StartCalibrationSequence(state_machine::Calibrating& calibrating)
    {
        calibrationOrchestrator.Start(
            calibrating.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                Dispatch(state_machine::CalibrationStepChanged{ step });
            },
            [this](foc::Radians angle)
            {
                Dispatch(state_machine::AlignmentSucceeded{ angle });
            },
            [this]
            {
                Dispatch(state_machine::CalibrationStepFailed{});
            });
    }

    void FocStateMachineCommon::StartAlignmentOnly(state_machine::Calibrating& calibrating)
    {
        calibrationOrchestrator.StartAlignmentOnly(
            calibrating.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                Dispatch(state_machine::CalibrationStepChanged{ step });
            },
            [this](foc::Radians angle)
            {
                Dispatch(state_machine::AlignmentSucceeded{ angle });
            },
            [this]
            {
                Dispatch(state_machine::CalibrationStepFailed{});
            });
    }

    void FocStateMachineCommon::OnAlignmentSucceeded(state_machine::Calibrating& calibrating, foc::Radians angle)
    {
        calibrating.pendingData.encoderZeroOffset = std::bit_cast<int32_t>(angle.Value());
        calibrationContext.SetRotorReferenceValid(true);

        if (calibrating.alignmentOnly)
            SaveCalibration(calibrating);
        else
            RunPostAlignmentStep(calibrating);
    }

    void FocStateMachineCommon::OnMechanicalParametersIdentified(state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
    {
        if (!event.friction || !event.inertia)
        {
            CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
            Dispatch(state_machine::FaultDetected{ state_machine::FaultCode::calibrationFailed });
            return;
        }

        calibrating.pendingData.inertia = event.inertia->Value();
        calibrating.pendingData.frictionViscous = event.friction->Value();
        calibrating.pendingData.speedLoopBandwidth = event.speedLoopBandwidth;
        SaveCalibration(calibrating);
    }

    void FocStateMachineCommon::SaveCalibration(state_machine::Calibrating& calibrating)
    {
        calibrating.pendingData.stage = HasValidModeSpecificCalibration(calibrating.pendingData)
                                            ? services::CalibrationStage::complete
                                            : services::CalibrationStage::none;

        nvm.SaveCalibration(calibrating.pendingData,
            [this](services::NvmStatus status)
            {
                Dispatch(state_machine::CalibrationSaved{ status });
            });
    }

    state_machine::Ready FocStateMachineCommon::CompleteCalibration(state_machine::Calibrating& calibrating)
    {
        auto data = calibrating.pendingData;

        calibrationContext.SetData(data);
        calibrationContext.Apply(GetFoc(), CurrentTunable());
        ApplyModeSpecificCalibration(data);
        CompleteAfterTransition(state_machine::CommandResult::ok);
        return BuildReady();
    }

    state_machine::Idle FocStateMachineCommon::CompletePartialCalibration(state_machine::Calibrating& calibrating)
    {
        tracer.Trace() << "[SM] Calibration incomplete for this mode; record kept as partial";
        calibrationContext.SetData(calibrating.pendingData);
        CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Idle{};
    }

    template<class Stopped>
    void FocStateMachineCommon::AddMaintenanceRowsFor()
    {
        stateMachine.AddInternal<Stopped, state_machine::ClearCalibration>(
            [this](Stopped&, const state_machine::ClearCalibration& command)
            {
                pendingCommandCallback = command.onDone;
                nvm.InvalidateCalibration([this](services::NvmStatus status)
                    {
                        Dispatch(state_machine::CalibrationInvalidated{ status });
                    });
            },
            [this](const Stopped&, const state_machine::ClearCalibration&)
            {
                return !HasPendingCommand();
            });

        stateMachine.Add<Stopped, state_machine::CalibrationInvalidated, state_machine::Idle>(
            [this](const Stopped&, const state_machine::CalibrationInvalidated& event)
            {
                return HasPendingCommand() && event.status == services::NvmStatus::Ok;
            },
            [this](Stopped&, const state_machine::CalibrationInvalidated&)
            {
                tracer.Trace() << "[SM] Calibration invalidated in NVM";
                calibrationContext.Invalidate();
                CompleteAfterTransition(state_machine::CommandResult::ok);
                return state_machine::Idle{};
            });

        stateMachine.Add<Stopped, state_machine::CalibrationInvalidated, state_machine::Fault>(
            [this](const Stopped&, const state_machine::CalibrationInvalidated& event)
            {
                return HasPendingCommand() && event.status != services::NvmStatus::Busy;
            },
            [this](Stopped&, const state_machine::CalibrationInvalidated&)
            {
                CompletePendingCommand(state_machine::CommandResult::nvmFailed);
                return BuildFault(state_machine::FaultCode::hardwareFault, false);
            });

        stateMachine.AddInternal<Stopped, state_machine::CalibrationInvalidated>([this](Stopped&, const state_machine::CalibrationInvalidated&)
            {
                CompletePendingCommand(state_machine::CommandResult::rejected);
            });

        stateMachine.AddInternal<Stopped, state_machine::SetFluxLinkage>(
            [this](Stopped&, const state_machine::SetFluxLinkage& command)
            {
                calibrationContext.SetPendingFluxLinkage(command.fluxLinkage.Value());
                pendingCommandCallback = command.onDone;

                auto updated = calibrationContext.Data();
                updated.fluxLinkage = calibrationContext.PendingFluxLinkage();

                nvm.SaveCalibration(updated, [this](services::NvmStatus status)
                    {
                        Dispatch(state_machine::FluxLinkageSaved{ status });
                    });
            },
            [this](const Stopped&, const state_machine::SetFluxLinkage& command)
            {
                if (command.fluxLinkage.Value() > 0.0f && !HasPendingCommand() && HasValidCalibration())
                    return true;

                tracer.Trace() << "[SM] Flux linkage rejected: needs a positive value and a calibrated motor in Idle or Ready";
                return false;
            });
    }

    template<class AnyState>
    void FocStateMachineCommon::AddFluxLinkageSavedRow()
    {
        stateMachine.AddInternal<AnyState, state_machine::FluxLinkageSaved>(
            [this](AnyState&, const state_machine::FluxLinkageSaved& event)
            {
                OnFluxLinkageSaved(event.status);
            },
            [this](const AnyState&, const state_machine::FluxLinkageSaved&)
            {
                return HasPendingCommand();
            });
    }

    void FocStateMachineCommon::AddMaintenanceRows()
    {
        AddMaintenanceRowsFor<state_machine::Idle>();
        AddMaintenanceRowsFor<state_machine::Ready>();

        AddFluxLinkageSavedRow<state_machine::Idle>();
        AddFluxLinkageSavedRow<state_machine::Calibrating>();
        AddFluxLinkageSavedRow<state_machine::Ready>();
        AddFluxLinkageSavedRow<state_machine::Enabled>();
        AddFluxLinkageSavedRow<state_machine::Fault>();
    }

    void FocStateMachineCommon::OnFluxLinkageSaved(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
        {
            tracer.Trace() << "[SM] Flux linkage not persisted";
            CompletePendingCommand(status == services::NvmStatus::Busy ? state_machine::CommandResult::rejected : state_machine::CommandResult::nvmFailed);
            return;
        }

        calibrationContext.CommitPendingFluxLinkage();
        calibrationContext.Apply(GetFoc(), CurrentTunable());
        tracer.Trace() << "[SM] Flux linkage stored";
        CompletePendingCommand(state_machine::CommandResult::ok);
    }

    void FocStateMachineCommon::CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (ToCommandResult(Dispatch(state_machine::ClearCalibration{ onDone })) != state_machine::CommandResult::ok)
            onDone(state_machine::CommandResult::rejected);
    }

    void FocStateMachineCommon::CmdSetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (ToCommandResult(Dispatch(state_machine::SetFluxLinkage{ fluxLinkage, onDone })) != state_machine::CommandResult::ok)
            onDone(state_machine::CommandResult::rejected);
    }

    foc::Weber FocStateMachineCommon::ActiveFluxLinkage() const
    {
        return calibrationContext.ActiveFluxLinkage();
    }

    void FocStateMachineCommon::CmdReAlign(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (ToCommandResult(Dispatch(state_machine::ReAlign{ onDone })) != state_machine::CommandResult::ok)
            onDone(state_machine::CommandResult::rejected);
    }

    state_machine::CommandResult FocStateMachineCommon::CmdReserveExternalCalibration()
    {
        return ToCommandResult(Dispatch(state_machine::ReserveExternalCalibration{}));
    }

    void FocStateMachineCommon::CmdCompleteExternalCalibration(const services::CalibrationData& data,
        const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (ToCommandResult(Dispatch(state_machine::CompleteExternalCalibration{ data, onDone })) != state_machine::CommandResult::ok)
            onDone(state_machine::CommandResult::rejected);
    }
}
