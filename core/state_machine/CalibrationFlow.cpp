#include "core/state_machine/CalibrationFlow.hpp"
#include <bit>

namespace application
{
    CalibrationFlow::CalibrationFlow(LifecycleMachine& machine,
        CalibrationContext& context,
        services::NonVolatileMemory& nvm,
        PendingCommand& pending,
        ModeHooks& mode,
        services::Tracer& tracer,
        services::ElectricalParametersIdentification& electricalIdent,
        services::MotorAlignment& motorAlignment)
        : machine(machine)
        , context(context)
        , nvm(nvm)
        , pending(pending)
        , mode(mode)
        , tracer(tracer)
        , orchestrator(electricalIdent, motorAlignment, tracer)
    {}

    state_machine::Calibrating CalibrationFlow::Begin(const state_machine::Calibrate& command)
    {
        pending.Accept(command.onDone);

        state_machine::Calibrating calibrating{};
        calibrating.pendingData.fluxLinkage = context.EffectiveFluxLinkage().Value();

        machine.Dispatch(state_machine::RunCalibrationSequence{});
        return calibrating;
    }

    state_machine::Calibrating CalibrationFlow::BeginReAlign(const state_machine::ReAlign& command)
    {
        pending.Accept(command.onDone);
        context.SetRotorReferenceValid(false);

        state_machine::Calibrating calibrating{};
        calibrating.pendingData = context.Data();
        calibrating.alignmentOnly = true;

        machine.Dispatch(state_machine::RunAlignmentOnly{});
        return calibrating;
    }

    state_machine::Calibrating CalibrationFlow::Reserve()
    {
        return state_machine::Calibrating{ state_machine::CalibrationStep::polePairs, {}, true };
    }

    bool CalibrationFlow::IsPlausibleExternal(const state_machine::CompleteExternalCalibration& command)
    {
        if (CalibrationContext::HasFiniteElectricalParameters(command.data))
            return true;

        tracer.Trace() << "[SM] External calibration rejected: implausible electrical data";
        return false;
    }

    void CalibrationFlow::BeginExternal(state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command)
    {
        pending.Accept(command.onDone);
        calibrating.pendingData = command.data;
        RunAlignmentOnly(calibrating);
    }

    void CalibrationFlow::RunSequence(state_machine::Calibrating& calibrating)
    {
        orchestrator.Start(
            calibrating.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                machine.Dispatch(state_machine::CalibrationStepChanged{ step });
            },
            [this](foc::Radians angle)
            {
                machine.Dispatch(state_machine::AlignmentSucceeded{ angle });
            },
            [this]
            {
                machine.Dispatch(state_machine::CalibrationStepFailed{});
            });
    }

    void CalibrationFlow::RunAlignmentOnly(state_machine::Calibrating& calibrating)
    {
        orchestrator.StartAlignmentOnly(
            calibrating.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                machine.Dispatch(state_machine::CalibrationStepChanged{ step });
            },
            [this](foc::Radians angle)
            {
                machine.Dispatch(state_machine::AlignmentSucceeded{ angle });
            },
            [this]
            {
                machine.Dispatch(state_machine::CalibrationStepFailed{});
            });
    }

    void CalibrationFlow::OnAlignmentSucceeded(state_machine::Calibrating& calibrating, foc::Radians angle)
    {
        calibrating.pendingData.encoderZeroOffset = std::bit_cast<int32_t>(angle.Value());
        context.SetRotorReferenceValid(true);

        if (calibrating.alignmentOnly)
            Save(calibrating);
        else
            mode.RunPostAlignmentStep(calibrating);
    }

    void CalibrationFlow::OnMechanicalParametersIdentified(state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
    {
        if (!event.friction || !event.inertia)
        {
            machine.Dispatch(state_machine::CalibrationStepFailed{});
            return;
        }

        calibrating.pendingData.inertia = event.inertia->Value();
        calibrating.pendingData.frictionViscous = event.friction->Value();
        calibrating.pendingData.speedLoopBandwidth = event.speedLoopBandwidth;
        Save(calibrating);
    }

    void CalibrationFlow::Save(state_machine::Calibrating& calibrating)
    {
        calibrating.pendingData.stage = mode.HasValidModeSpecificCalibration(calibrating.pendingData)
                                            ? services::CalibrationStage::complete
                                            : services::CalibrationStage::none;

        nvm.SaveCalibration(calibrating.pendingData, machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
                                                         {
                                                             return state_machine::CalibrationSaved{ status };
                                                         }));
    }

    state_machine::Ready CalibrationFlow::Complete(const state_machine::Calibrating& calibrating)
    {
        const auto& data = calibrating.pendingData;

        context.SetData(data);
        context.Apply(mode.GetFoc(), mode.CurrentTunable());
        mode.ApplyModeSpecificCalibration(data);
        pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return ReadyState();
    }

    state_machine::Idle CalibrationFlow::CompletePartial(const state_machine::Calibrating& calibrating)
    {
        tracer.Trace() << "[SM] Calibration incomplete for this mode; record kept as partial";
        context.SetData(calibrating.pendingData);
        pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Idle{};
    }

    state_machine::Ready CalibrationFlow::ReadyState() const
    {
        return state_machine::Ready{ context.Data(), context.IsRotorReferenceValid() };
    }

    bool CalibrationFlow::HasValidCalibration() const
    {
        return context.IsComplete(mode.HasValidModeSpecificCalibration(context.Data()));
    }

    bool CalibrationFlow::IsRunning() const
    {
        return orchestrator.IsRunning();
    }

    void CalibrationFlow::Abort()
    {
        orchestrator.Abort();
        mode.AbortModeSpecificServices();
    }
}
