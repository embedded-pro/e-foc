#include "core/state_machine/FocStateMachineCommon.hpp"
#include <bit>

namespace application
{
    void FocStateMachineCommon::CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (!state_machine::IsStopped(currentState) || HasPendingCommand())
        {
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        pendingCommandCallback = onDone;
        nvm.InvalidateCalibration([this](services::NvmStatus status)
            {
                OnCalibrationInvalidated(status);
            });
    }

    void FocStateMachineCommon::OnCalibrationInvalidated(services::NvmStatus status)
    {
        if (!HasPendingCommand() || !state_machine::IsStopped(currentState))
            return;

        if (status == services::NvmStatus::Busy)
        {
            CompletePendingCommand(state_machine::CommandResult::rejected);
            return;
        }

        if (status != services::NvmStatus::Ok)
        {
            CompletePendingCommand(state_machine::CommandResult::nvmFailed);
            EnterFault(state_machine::FaultCode::hardwareFault);
        }
        else
        {
            tracer.Trace() << "[SM] Calibration invalidated in NVM";
            calibrationContext.Invalidate();
            currentState = state_machine::Idle{};
            CompletePendingCommand(state_machine::CommandResult::ok);
        }
    }

    void FocStateMachineCommon::OnCalibrationComplete()
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return;

        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.pendingData.stage = HasValidModeSpecificCalibration(calibrating.pendingData)
                                            ? services::CalibrationStage::complete
                                            : services::CalibrationStage::none;

        nvm.SaveCalibration(calibrating.pendingData,
            [this](services::NvmStatus status)
            {
                OnCalibrationSaved(status);
            });
    }

    void FocStateMachineCommon::OnCalibrationSaved(services::NvmStatus status)
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return;

        if (status != services::NvmStatus::Ok)
        {
            CompletePendingCommand(state_machine::CommandResult::nvmFailed);
            EnterFault(state_machine::FaultCode::calibrationFailed);
        }
        else
        {
            auto data = std::get<state_machine::Calibrating>(currentState).pendingData;

            if (data.stage == services::CalibrationStage::complete)
            {
                calibrationContext.SetData(data);
                calibrationContext.Apply(GetFoc(), CurrentTunable());
                ApplyModeSpecificCalibration(data);
                EnterReady(data);
            }
            else
                EnterIdleWithPartialCalibration(data);

            CompletePendingCommand(state_machine::CommandResult::ok);
        }
    }

    state_machine::CommandResult FocStateMachineCommon::CmdReserveExternalCalibration()
    {
        if (!state_machine::IsStopped(currentState) || HasPendingAsyncWork())
            return state_machine::CommandResult::rejected;

        tracer.Trace() << "[SM] Entering Calibrating (external)";
        currentState = state_machine::Calibrating{};
        std::get<state_machine::Calibrating>(currentState).external = true;
        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::CmdCompleteExternalCalibration(const services::CalibrationData& data,
        const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
        {
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        if (!CalibrationContext::HasFiniteElectricalParameters(data))
        {
            tracer.Trace() << "[SM] External calibration rejected: implausible electrical data";
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        pendingCommandCallback = onDone;
        std::get<state_machine::Calibrating>(currentState).pendingData = data;
        RunAlignmentStep();
    }

    void FocStateMachineCommon::CmdSetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (fluxLinkage.Value() <= 0.0f || !state_machine::IsStopped(currentState) || HasPendingCommand() || !HasValidCalibration())
        {
            tracer.Trace() << "[SM] Flux linkage rejected: needs a positive value and a calibrated motor in Idle or Ready";
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        calibrationContext.SetPendingFluxLinkage(fluxLinkage.Value());
        pendingCommandCallback = onDone;

        auto updated = calibrationContext.Data();
        updated.fluxLinkage = calibrationContext.PendingFluxLinkage();

        nvm.SaveCalibration(updated, [this](services::NvmStatus status)
            {
                OnFluxLinkageSaved(status);
            });
    }

    void FocStateMachineCommon::OnFluxLinkageSaved(services::NvmStatus status)
    {
        if (!HasPendingCommand())
            return;

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

    foc::Weber FocStateMachineCommon::ActiveFluxLinkage() const
    {
        return calibrationContext.ActiveFluxLinkage();
    }

    void FocStateMachineCommon::CmdReAlign(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (!state_machine::IsStopped(currentState) || HasPendingCommand() || !HasValidCalibration())
        {
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        pendingCommandCallback = onDone;
        calibrationContext.SetRotorReferenceValid(false);
        currentState = state_machine::Calibrating{};
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.pendingData = calibrationContext.Data();
        calibrating.external = true;

        RunAlignmentStep();
    }
}
