#include "core/state_machine/BootSequence.hpp"

namespace application
{
    BootSequence::BootSequence(const LifecycleEnvironment& environment, const CalibrationFlow& calibration)
        : env(environment)
        , calibration(calibration)
    {}

    void BootSequence::Begin()
    {
        env.nvmActivity.Begin();
        env.nvm.IsCalibrationValid([this](bool valid)
            {
                OnValidityChecked(valid);
            });
    }

    void BootSequence::Continue(bool valid)
    {
        if (!valid)
        {
            env.context.SetRotorReferenceValid(false);
            env.tracer.Trace() << "[SM] NVM invalid, starting in Idle";
            return;
        }

        env.nvmActivity.Begin();
        env.nvm.LoadCalibration(env.context.MutableData(), [this](services::NvmStatus status)
            {
                OnCalibrationLoaded(status);
            });
    }

    void BootSequence::Finish(services::NvmStatus status)
    {
        // A reference kept by a control-mode switch survives only into Ready (FinishAligned).
        env.context.SetRotorReferenceValid(false);

        if (status != services::NvmStatus::Ok)
            env.tracer.Trace() << "[SM] NVM load failed, starting in Idle";
        else if (!calibration.HasValidCalibration())
        {
            env.tracer.Trace() << "[SM] NVM data incomplete, starting in Idle";
            env.context.ApplyCurrentOffsets();
        }
        else
        {
            env.tracer.Trace() << "[SM] Electrical parameters restored; run alignment before enabling";
            ApplyRecord();
        }
    }

    bool BootSequence::ResumesAligned(services::NvmStatus status) const
    {
        return status == services::NvmStatus::Ok && env.context.IsRotorReferenceValid() && calibration.HasValidCalibration();
    }

    state_machine::Ready BootSequence::FinishAligned()
    {
        env.tracer.Trace() << "[SM] Electrical parameters restored; rotor reference kept across the control-mode switch";
        ApplyRecord();
        return calibration.ReadyState();
    }

    void BootSequence::ApplyRecord()
    {
        env.context.Apply(env.mode.GetFoc(), env.mode.CurrentTunable());
        env.mode.ApplyModeSpecificCalibration(env.context.Data());
    }

    void BootSequence::OnValidityChecked(bool valid)
    {
        env.nvmActivity.End();
        env.machine.Dispatch(state_machine::BootValidityChecked{ valid });
    }

    void BootSequence::OnCalibrationLoaded(services::NvmStatus status)
    {
        env.nvmActivity.End();
        env.machine.Dispatch(state_machine::BootCalibrationLoaded{ status });
    }
}
