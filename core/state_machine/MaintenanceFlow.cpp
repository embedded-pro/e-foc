#include "core/state_machine/MaintenanceFlow.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <cmath>
#include <limits>

namespace application
{
    MaintenanceFlow::MaintenanceFlow(const LifecycleEnvironment& environment, const CalibrationFlow& calibration)
        : env(environment)
        , calibration(calibration)
    {}

    void MaintenanceFlow::BeginClear(const state_machine::ClearCalibration& command)
    {
        env.pending.Accept(command.onDone);
        env.nvmActivity.Begin();
        env.nvm.InvalidateCalibration([this](services::NvmStatus status)
            {
                OnInvalidated(status);
            });
    }

    state_machine::Idle MaintenanceFlow::CompleteClear()
    {
        env.tracer.Trace() << "[SM] Calibration invalidated in NVM";
        env.context.Invalidate();
        env.pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Idle{};
    }

    void MaintenanceFlow::ReconcileClear()
    {
        env.tracer.Trace() << "[SM] Calibration invalidated in NVM while faulted; record dropped";
        env.context.Invalidate();
    }

    void MaintenanceFlow::RejectClear()
    {
        env.pending.Complete(state_machine::CommandResult::rejected);
    }

    bool MaintenanceFlow::IsAcceptableMotorParameter(const state_machine::SetMotorParameter& command) const
    {
        const auto value = command.value;
        bool acceptable{ false };

        switch (command.parameter)
        {
            case services::MotorParameter::polePairs:
                acceptable = foc::IsWithinInclusive(value, 1.0f, static_cast<float>(std::numeric_limits<uint8_t>::max())) && std::round(value) == value;
                break;
            case services::MotorParameter::fluxLinkage:
            case services::MotorParameter::viscousFriction:
                acceptable = foc::IsFiniteNonNegative(value);
                break;
            default:
                acceptable = foc::IsFinitePositive(value);
                break;
        }

        if (!acceptable)
            env.tracer.Trace() << "[SM] Motor parameter rejected: not a physical value";

        return acceptable;
    }

    void MaintenanceFlow::BeginSetMotorParameter(const state_machine::SetMotorParameter& command)
    {
        pendingParameter = command.parameter;
        pendingValue = command.value;
        env.pending.Accept(command.onDone);

        env.nvmActivity.Begin();
        env.nvm.SaveCalibration(Edited(env.context.Data()), [this](services::NvmStatus status)
            {
                OnMotorParameterStored(status);
            });
    }

    // The record is adopted only once the NVM holds it, so RAM never runs a value a reset would lose
    void MaintenanceFlow::AdoptMotorParameter(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
        {
            env.tracer.Trace() << "[SM] Motor parameter not persisted";
            return;
        }

        const auto previous = env.context.Data();
        env.context.SetData(Edited(previous));

        if (env.context.Data().polePairs != previous.polePairs)
        {
            env.context.SetRotorReferenceValid(false);
            env.tracer.Trace() << "[SM] Pole pairs changed; align the rotor before enabling";
        }

        if (calibration.HasValidCalibration())
        {
            env.context.Apply(env.mode.GetFoc(), env.mode.CurrentTunable());
            env.mode.ApplyModeSpecificCalibration(env.context.Data());
        }

        env.tracer.Trace() << "[SM] Motor parameter stored";
    }

    void MaintenanceFlow::CompleteMotorParameter(services::NvmStatus status)
    {
        if (status == services::NvmStatus::Ok)
            env.pending.Complete(state_machine::CommandResult::ok);
        else
            env.pending.Complete(status == services::NvmStatus::Busy ? state_machine::CommandResult::rejected : state_machine::CommandResult::nvmFailed);
    }

    void MaintenanceFlow::OnInvalidated(services::NvmStatus status)
    {
        env.nvmActivity.End();
        env.machine.Dispatch(state_machine::CalibrationInvalidated{ status });
    }

    void MaintenanceFlow::OnMotorParameterStored(services::NvmStatus status)
    {
        env.nvmActivity.End();
        env.machine.Dispatch(state_machine::MotorParameterSaved{ status });
    }

    services::CalibrationData MaintenanceFlow::Edited(const services::CalibrationData& data) const
    {
        auto edited = data;
        services::WriteMotorParameter(edited, pendingParameter, pendingValue);

        if (data.stage == services::CalibrationStage::complete || CompletesRecord(edited))
            edited.stage = services::CalibrationStage::complete;

        return edited;
    }

    bool MaintenanceFlow::CompletesRecord(const services::CalibrationData& data) const
    {
        return data.polePairs != 0 && foc::IsFinitePositive(data.lD) && CalibrationContext::HasFiniteElectricalParameters(data) &&
               env.mode.HasValidModeSpecificCalibration(data);
    }
}
