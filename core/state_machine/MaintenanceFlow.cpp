#include "core/state_machine/MaintenanceFlow.hpp"

namespace application
{
    MaintenanceFlow::MaintenanceFlow(LifecycleMachine& machine,
        CalibrationContext& context,
        services::NonVolatileMemory& nvm,
        PendingCommand& pending,
        ModeHooks& mode,
        services::Tracer& tracer,
        const CalibrationFlow& calibration)
        : machine(machine)
        , context(context)
        , nvm(nvm)
        , pending(pending)
        , mode(mode)
        , tracer(tracer)
        , calibration(calibration)
    {}

    void MaintenanceFlow::BeginClear(const state_machine::ClearCalibration& command)
    {
        pending.Accept(command.onDone);
        nvm.InvalidateCalibration(machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
            {
                return state_machine::CalibrationInvalidated{ status };
            }));
    }

    state_machine::Idle MaintenanceFlow::CompleteClear()
    {
        tracer.Trace() << "[SM] Calibration invalidated in NVM";
        context.Invalidate();
        pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Idle{};
    }

    void MaintenanceFlow::RejectClear()
    {
        pending.Complete(state_machine::CommandResult::rejected);
    }

    bool MaintenanceFlow::IsAcceptableFluxLinkage(const state_machine::SetFluxLinkage& command) const
    {
        if (command.fluxLinkage.Value() > 0.0f && !pending.Pending() && calibration.HasValidCalibration())
            return true;

        tracer.Trace() << "[SM] Flux linkage rejected: needs a positive value and a calibrated motor in Idle or Ready";
        return false;
    }

    void MaintenanceFlow::BeginSetFluxLinkage(const state_machine::SetFluxLinkage& command)
    {
        context.SetPendingFluxLinkage(command.fluxLinkage.Value());
        pending.Accept(command.onDone);

        auto updated = context.Data();
        updated.fluxLinkage = context.PendingFluxLinkage();

        nvm.SaveCalibration(updated, machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
                                         {
                                             return state_machine::FluxLinkageSaved{ status };
                                         }));
    }

    void MaintenanceFlow::OnFluxLinkageSaved(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
        {
            tracer.Trace() << "[SM] Flux linkage not persisted";
            pending.Complete(status == services::NvmStatus::Busy ? state_machine::CommandResult::rejected : state_machine::CommandResult::nvmFailed);
            return;
        }

        context.CommitPendingFluxLinkage();
        context.Apply(mode.GetFoc(), mode.CurrentTunable());
        tracer.Trace() << "[SM] Flux linkage stored";
        pending.Complete(state_machine::CommandResult::ok);
    }
}
