#include "core/state_machine/CalibrationFlow.hpp"
#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <bit>

namespace application
{
    CalibrationFlow::CalibrationFlow(const LifecycleEnvironment& environment, const CalibrationServices& services)
        : env(environment)
        , orchestrator(services.electricalIdent, services.motorAlignment, environment.tracer)
    {}

    state_machine::Calibrating CalibrationFlow::Begin(const state_machine::Calibrate& command)
    {
        env.pending.Accept(command.onDone);

        state_machine::Calibrating calibrating{};
        calibrating.pendingData.fluxLinkage = env.context.EffectiveFluxLinkage().Value();

        env.machine.Dispatch(state_machine::RunCalibrationSequence{});
        return calibrating;
    }

    state_machine::Calibrating CalibrationFlow::BeginReAlign(const state_machine::ReAlign& command)
    {
        env.pending.Accept(command.onDone);
        env.context.SetRotorReferenceValid(false);

        state_machine::Calibrating calibrating{};
        calibrating.pendingData = env.context.Data();
        calibrating.alignmentOnly = true;

        env.machine.Dispatch(state_machine::RunAlignmentOnly{});
        return calibrating;
    }

    state_machine::Calibrating CalibrationFlow::Reserve()
    {
        return state_machine::Calibrating{ state_machine::CalibrationStep::polePairs, {}, true };
    }

    bool CalibrationFlow::IsPlausibleExternal(const state_machine::CompleteExternalCalibration& command) const
    {
        if (CalibrationContext::HasFiniteElectricalParameters(command.data))
            return true;

        env.tracer.Trace() << "[SM] External calibration rejected: implausible electrical data";
        return false;
    }

    void CalibrationFlow::BeginExternal(state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command)
    {
        env.pending.Accept(command.onDone);
        calibrating.pendingData = command.data;
        RunAlignmentOnly(calibrating);
    }

    void CalibrationFlow::RunSequence(state_machine::Calibrating& calibrating)
    {
        orchestrator.Start(
            calibrating.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                env.machine.Dispatch(state_machine::CalibrationStepChanged{ step });
            },
            [this](foc::Radians angle)
            {
                env.machine.Dispatch(state_machine::AlignmentSucceeded{ angle });
            },
            [this]
            {
                env.machine.Dispatch(state_machine::CalibrationStepFailed{});
            });
    }

    void CalibrationFlow::RunAlignmentOnly(state_machine::Calibrating& calibrating)
    {
        orchestrator.StartAlignmentOnly(
            calibrating.pendingData,
            [this](state_machine::CalibrationStep step)
            {
                env.machine.Dispatch(state_machine::CalibrationStepChanged{ step });
            },
            [this](foc::Radians angle)
            {
                env.machine.Dispatch(state_machine::AlignmentSucceeded{ angle });
            },
            [this]
            {
                env.machine.Dispatch(state_machine::CalibrationStepFailed{});
            });
    }

    void CalibrationFlow::OnAlignmentSucceeded(state_machine::Calibrating& calibrating, foc::Radians angle)
    {
        calibrating.pendingData.encoderZeroOffset = std::bit_cast<int32_t>(angle.Value());
        env.context.SetRotorReferenceValid(true);

        if (calibrating.alignmentOnly)
            Save(calibrating);
        else
            env.mode.RunPostAlignmentStep(calibrating);
    }

    void CalibrationFlow::OnMechanicalParametersIdentified(state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
    {
        if (!event.friction || !event.inertia || !services::IsPlausibleMechanics(event.inertia->Value(), event.friction->Value()))
        {
            env.tracer.Trace() << "[SM] Mechanical identification produced no usable estimate";
            env.machine.Dispatch(state_machine::CalibrationStepFailed{});
            return;
        }

        calibrating.pendingData.inertia = event.inertia->Value();
        calibrating.pendingData.frictionViscous = event.friction->Value();
        calibrating.pendingData.speedLoopBandwidth = event.speedLoopBandwidth;
        Save(calibrating);
    }

    void CalibrationFlow::Save(state_machine::Calibrating& calibrating)
    {
        calibrating.pendingData.stage = env.mode.HasValidModeSpecificCalibration(calibrating.pendingData)
                                            ? services::CalibrationStage::complete
                                            : services::CalibrationStage::none;

        really_assert(saveCompletion == nullptr);
        saveCompletion = env.machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
            {
                return state_machine::CalibrationSaved{ status };
            });
        env.nvmActivity.Begin();
        env.nvm.SaveCalibration(calibrating.pendingData, [this](services::NvmStatus status)
            {
                OnSaved(status);
            });
    }

    void CalibrationFlow::OnSaved(services::NvmStatus status)
    {
        env.nvmActivity.End();
        auto completion = saveCompletion;
        saveCompletion = nullptr;
        completion(status);
    }

    state_machine::Ready CalibrationFlow::Complete(const state_machine::Calibrating& calibrating)
    {
        const auto& data = calibrating.pendingData;

        env.mode.ProvisionalControlSuperseded();
        env.context.SetData(data);
        env.context.Apply(env.mode.GetFoc(), env.mode.CurrentTunable());
        env.mode.ApplyModeSpecificCalibration(data);
        env.pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return ReadyState();
    }

    state_machine::Idle CalibrationFlow::CompletePartial(const state_machine::Calibrating& calibrating)
    {
        env.tracer.Trace() << "[SM] Calibration incomplete for this mode; record kept as partial";
        env.mode.RestoreControlAfterProvisionalIdentification();
        env.context.SetData(calibrating.pendingData);
        env.pending.CompleteAfterTransition(state_machine::CommandResult::ok);
        return state_machine::Idle{};
    }

    state_machine::Ready CalibrationFlow::ReadyState() const
    {
        return state_machine::Ready{ env.context.Data(), env.context.IsRotorReferenceValid() };
    }

    bool CalibrationFlow::HasValidCalibration() const
    {
        return env.context.IsComplete(env.mode.HasValidModeSpecificCalibration(env.context.Data()));
    }

    bool CalibrationFlow::IsRunning() const
    {
        return orchestrator.IsRunning();
    }

    bool CalibrationFlow::HasRunInFlight() const
    {
        return orchestrator.HasRunInFlight();
    }

    void CalibrationFlow::Abort()
    {
        orchestrator.Abort();
        env.mode.AbortModeSpecificServices();
        env.mode.RestoreControlAfterProvisionalIdentification();
    }
}
