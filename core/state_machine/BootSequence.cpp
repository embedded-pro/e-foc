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
        if (status != services::NvmStatus::Ok)
            env.tracer.Trace() << "[SM] NVM load failed, starting in Idle";
        else if (!calibration.HasValidCalibration())
            env.tracer.Trace() << "[SM] NVM data incomplete, starting in Idle";
        else
        {
            env.tracer.Trace() << "[SM] Electrical parameters restored; run alignment before enabling";
            env.context.Apply(env.mode.GetFoc(), env.mode.CurrentTunable());
            env.mode.ApplyModeSpecificCalibration(env.context.Data());
        }
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
