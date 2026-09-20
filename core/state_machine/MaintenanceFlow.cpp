#include "core/state_machine/MaintenanceFlow.hpp"

namespace application
{
    MaintenanceFlow::MaintenanceFlow(const LifecycleEnvironment& environment, const CalibrationFlow& calibration)
        : env(environment)
        , calibration(calibration)
    {}

    void MaintenanceFlow::BeginClear(const state_machine::ClearCalibration& command)
    {
        env.pending.Accept(command.onDone);
        completion = env.machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
            {
                return state_machine::CalibrationInvalidated{ status };
            });
        env.nvmActivity.Begin();
        env.nvm.InvalidateCalibration([this](services::NvmStatus status)
            {
                OnNvmDone(status);
            });
    }

    state_machine::Idle MaintenanceFlow::CompleteClear()
    {
        env.tracer.Trace() << "[SM] Calibration invalidated in NVM";
        env.context.Invalidate();
        env.pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Idle{};
    }

    void MaintenanceFlow::RejectClear()
    {
        env.pending.Complete(state_machine::CommandResult::rejected);
    }

    bool MaintenanceFlow::IsAcceptableFluxLinkage(const state_machine::SetFluxLinkage& command) const
    {
        if (command.fluxLinkage.Value() > 0.0f && !env.pending.Pending() && calibration.HasValidCalibration())
            return true;

        env.tracer.Trace() << "[SM] Flux linkage rejected: needs a positive value and a calibrated motor in Idle or Ready";
        return false;
    }

    void MaintenanceFlow::BeginSetFluxLinkage(const state_machine::SetFluxLinkage& command)
    {
        env.context.SetPendingFluxLinkage(command.fluxLinkage.Value());
        env.pending.Accept(command.onDone);

        auto updated = env.context.Data();
        updated.fluxLinkage = env.context.PendingFluxLinkage();

        completion = env.machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
            {
                return state_machine::FluxLinkageSaved{ status };
            });
        env.nvmActivity.Begin();
        env.nvm.SaveCalibration(updated, [this](services::NvmStatus status)
            {
                OnNvmDone(status);
            });
    }

    void MaintenanceFlow::OnFluxLinkageSaved(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
        {
            env.tracer.Trace() << "[SM] Flux linkage not persisted";
            env.pending.Complete(status == services::NvmStatus::Busy ? state_machine::CommandResult::rejected : state_machine::CommandResult::nvmFailed);
            return;
        }

        env.context.CommitPendingFluxLinkage();
        env.context.Apply(env.mode.GetFoc(), env.mode.CurrentTunable());
        env.tracer.Trace() << "[SM] Flux linkage stored";
        env.pending.Complete(state_machine::CommandResult::ok);
    }

    void MaintenanceFlow::OnNvmDone(services::NvmStatus status)
    {
        env.nvmActivity.End();
        auto done = completion;
        completion = nullptr;
        done(status);
    }
}
