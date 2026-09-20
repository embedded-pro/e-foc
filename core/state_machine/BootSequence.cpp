#include "core/state_machine/BootSequence.hpp"

namespace application
{
    BootSequence::BootSequence(LifecycleMachine& machine,
        CalibrationContext& context,
        services::NonVolatileMemory& nvm,
        ModeHooks& mode,
        services::Tracer& tracer,
        const CalibrationFlow& calibration)
        : machine(machine)
        , context(context)
        , nvm(nvm)
        , mode(mode)
        , tracer(tracer)
        , calibration(calibration)
    {}

    void BootSequence::Begin()
    {
        inFlight = true;
        nvm.IsCalibrationValid([this](bool valid)
            {
                OnValidityChecked(valid);
            });
    }

    void BootSequence::Continue(bool valid)
    {
        if (!valid)
        {
            tracer.Trace() << "[SM] NVM invalid, starting in Idle";
            return;
        }

        inFlight = true;
        nvm.LoadCalibration(context.MutableData(), [this](services::NvmStatus status)
            {
                OnCalibrationLoaded(status);
            });
    }

    void BootSequence::Finish(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
            tracer.Trace() << "[SM] NVM load failed, starting in Idle";
        else if (!calibration.HasValidCalibration())
            tracer.Trace() << "[SM] NVM data incomplete, starting in Idle";
        else
        {
            tracer.Trace() << "[SM] Electrical parameters restored; run alignment before enabling";
            context.Apply(mode.GetFoc(), mode.CurrentTunable());
            mode.ApplyModeSpecificCalibration(context.Data());
        }
    }

    bool BootSequence::InFlight() const
    {
        return inFlight;
    }

    void BootSequence::OnValidityChecked(bool valid)
    {
        inFlight = false;
        machine.Dispatch(state_machine::BootValidityChecked{ valid });
    }

    void BootSequence::OnCalibrationLoaded(services::NvmStatus status)
    {
        inFlight = false;
        machine.Dispatch(state_machine::BootCalibrationLoaded{ status });
    }
}
