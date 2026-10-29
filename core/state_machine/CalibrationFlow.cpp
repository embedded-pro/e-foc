#include "core/state_machine/CalibrationFlow.hpp"
#include "core/services/cogging_compensation/CoggingMap.hpp"
#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <bit>
#include <cstdint>

namespace
{
    constexpr float microPerUnit = 1.0e6f;
    constexpr float milliPerUnit = 1.0e3f;
}

namespace application
{
    CalibrationFlow::CalibrationFlow(const LifecycleEnvironment& environment, const CalibrationServices& services)
        : env(environment)
        , agreeingRuns(services.agreeingMechanicalRuns)
        , orchestrator(services.electricalIdent, services.motorAlignment, environment.tracer, services.currentOffsets, services.standstill)
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
        state_machine::Calibrating calibrating{ state_machine::CalibrationStep::polePairs, {}, true };
        calibrating.externalReservation = true;
        return calibrating;
    }

    bool CalibrationFlow::IsReleasable(const state_machine::Calibrating& calibrating) const
    {
        return calibrating.externalReservation && !env.pending.Pending() && !HasRunInFlight();
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
        const auto& held = env.context.Data();

        env.pending.Accept(command.onDone);
        calibrating.pendingData = command.data;
        calibrating.pendingData.coggingTorque = held.coggingTorque;
        calibrating.pendingData.coggingTorqueScale = held.coggingTorqueScale;
        calibrating.pendingData.inertia = held.inertia;
        calibrating.pendingData.frictionViscous = held.frictionViscous;
        calibrating.pendingData.frictionCoulomb = held.frictionCoulomb;
        calibrating.pendingData.currentLoopBandwidth = held.currentLoopBandwidth;
        calibrating.pendingData.speedLoopBandwidth = held.speedLoopBandwidth;
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

    std::optional<state_machine::MechanicalRun> CalibrationFlow::AgreedMechanicalRun(const state_machine::Calibrating& calibrating) const
    {
        const auto count = calibrating.mechanicalRunCount;

        if (agreeingRuns <= 1)
            return count > 0 ? std::optional{ calibrating.mechanicalRuns[count - 1] } : std::nullopt;

        for (std::size_t first = 0; first != count; ++first)
        {
            for (std::size_t second = first + 1; second != count; ++second)
            {
                const auto& a = calibrating.mechanicalRuns[first];
                const auto& b = calibrating.mechanicalRuns[second];

                if (!services::EstimatesAgree(a.inertia, b.inertia))
                    continue;

                return state_machine::MechanicalRun{ 0.5f * (a.inertia + b.inertia), 0.5f * (a.friction + b.friction) };
            }
        }

        return std::nullopt;
    }

    void CalibrationFlow::OnMechanicalParametersIdentified(state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event)
    {
        const bool usable = event.friction && event.inertia && services::IsPlausibleMechanics(event.inertia->Value(), event.friction->Value());

        if (usable)
        {
            calibrating.mechanicalRuns[calibrating.mechanicalRunCount++] = state_machine::MechanicalRun{ event.inertia->Value(), event.friction->Value() };
            env.tracer.Trace() << "[SM] Mechanical run " << static_cast<uint32_t>(calibrating.mechanicalRunCount) << ": J_uNms2=" << event.inertia->Value() * microPerUnit << " B_uNms=" << event.friction->Value() * microPerUnit;
        }
        else
            env.tracer.Trace() << "[SM] Mechanical identification produced no usable estimate";

        if (const auto agreed = AgreedMechanicalRun(calibrating))
        {
            calibrating.pendingData.inertia = agreed->inertia;
            calibrating.pendingData.frictionViscous = agreed->friction;
            calibrating.pendingData.speedLoopBandwidth = event.speedLoopBandwidth;
            if (!env.mode.RunPostMechanicalStep(calibrating))
                Save(calibrating);
            return;
        }

        if (usable)
            env.tracer.Trace() << "[SM] Mechanical estimate J_uNms2=" << event.inertia->Value() * microPerUnit << " not confirmed by a second run";

        if (calibrating.mechanicalAttempts + 1 < maxMechanicalAttempts)
        {
            ++calibrating.mechanicalAttempts;
            env.tracer.Trace() << "[SM] Aligning again for mechanical identification, attempt " << static_cast<uint32_t>(calibrating.mechanicalAttempts + 1) << " of " << static_cast<uint32_t>(maxMechanicalAttempts);
            RunAlignmentOnly(calibrating);
            return;
        }

        env.machine.Dispatch(state_machine::CalibrationStepFailed{});
    }

    void CalibrationFlow::OnCoggingMapped(state_machine::Calibrating& calibrating, const state_machine::CoggingMapped& event)
    {
        if (event.map)
        {
            calibrating.pendingData.coggingTorque = event.map->torque;
            calibrating.pendingData.coggingTorqueScale = event.map->torqueScale;
            env.tracer.Trace() << "[SM] Cogging map: peak_mNm=" << event.map->peak.Value() * milliPerUnit << " agreement=" << event.map->agreement;
        }
        else
            env.tracer.Trace() << "[SM] Cogging map: none";

        Save(calibrating);
    }

    void CalibrationFlow::Save(state_machine::Calibrating& calibrating)
    {
        calibrating.pendingData.stage = env.mode.HasValidModeSpecificCalibration(calibrating.pendingData)
                                            ? services::CalibrationStage::complete
                                            : services::CalibrationStage::none;

        TraceRecord(calibrating.pendingData);

        really_assert(saveCompletion == nullptr);
        saveCompletion = env.machine.CompletionWith<void(services::NvmStatus)>([](services::NvmStatus status)
            {
                return state_machine::CalibrationSaved{ status };
            });
        pendingSaveData = calibrating.pendingData;
        env.nvmActivity.Begin();
        env.nvm.SaveCalibration(pendingSaveData, [this](services::NvmStatus status)
            {
                OnSaved(status);
            });
    }

    // Inertia and friction are traced in micro-units: the tracer prints only three decimals.
    void CalibrationFlow::TraceRecord(const services::CalibrationData& data) const
    {
        env.tracer.Trace() << "[SM] Calibration record: R=" << data.rPhase
                           << " L_mH=" << data.lD
                           << " p=" << static_cast<uint32_t>(data.polePairs)
                           << " J_uNms2=" << data.inertia * microPerUnit
                           << " B_uNms=" << data.frictionViscous * microPerUnit
                           << " Ia_mA=" << data.currentOffsetA * milliPerUnit
                           << " Ib_mA=" << data.currentOffsetB * milliPerUnit
                           << " Ic_mA=" << data.currentOffsetC * milliPerUnit
                           << " In_mA=" << static_cast<uint32_t>(data.currentNoiseMilliAmpere)
                           << " cog_mNm=" << services::CoggingPeak(data.coggingTorque, data.coggingTorqueScale).Value() * milliPerUnit;
    }

    // Busy is contention with another writer of the shared NVM, not a failure of this record
    void CalibrationFlow::OnSaved(services::NvmStatus status)
    {
        if (saveCompletion == nullptr)
        {
            env.nvmActivity.End();
            return;
        }

        if (status == services::NvmStatus::Busy)
        {
            RetrySave();
            return;
        }

        env.nvmActivity.End();
        auto completion = saveCompletion;
        saveCompletion = nullptr;
        completion(status);
    }

    void CalibrationFlow::RetrySave()
    {
        saveRetryTimer.Start(saveRetryDelay, [this]()
            {
                env.nvm.SaveCalibration(pendingSaveData, [this](services::NvmStatus status)
                    {
                        OnSaved(status);
                    });
            });
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
        if (saveRetryTimer.Armed())
        {
            saveRetryTimer.Cancel();
            env.nvmActivity.End();
        }

        // A save still in flight answers into OnSaved, which then neither retries nor completes the aborted run
        saveCompletion = nullptr;

        orchestrator.Abort();
        env.mode.AbortModeSpecificServices();
        env.mode.RestoreControlAfterProvisionalIdentification();
    }
}
