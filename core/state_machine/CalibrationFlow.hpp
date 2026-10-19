#pragma once

#include "core/state_machine/CalibrationOrchestrator.hpp"
#include "core/state_machine/FocStateMachineDependencies.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "infra/timer/Timer.hpp"
#include <optional>

namespace application
{
    class CalibrationFlow
    {
    public:
        CalibrationFlow(const LifecycleEnvironment& environment, const CalibrationServices& services);

        state_machine::Calibrating Begin(const state_machine::Calibrate& command);
        state_machine::Calibrating BeginReAlign(const state_machine::ReAlign& command);
        static state_machine::Calibrating Reserve();

        bool IsPlausibleExternal(const state_machine::CompleteExternalCalibration& command) const;
        void BeginExternal(state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command);

        void RunSequence(state_machine::Calibrating& calibrating);
        void RunAlignmentOnly(state_machine::Calibrating& calibrating);
        void OnAlignmentSucceeded(state_machine::Calibrating& calibrating, foc::Radians angle);
        void OnMechanicalParametersIdentified(state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event);
        void Save(state_machine::Calibrating& calibrating);

        state_machine::Ready Complete(const state_machine::Calibrating& calibrating);
        state_machine::Idle CompletePartial(const state_machine::Calibrating& calibrating);
        state_machine::Ready ReadyState() const;

        bool HasValidCalibration() const;
        bool IsRunning() const;
        bool HasRunInFlight() const;
        void Abort();

    private:
        void TraceRecord(const services::CalibrationData& data) const;
        void OnSaved(services::NvmStatus status);
        void RetrySave();
        std::optional<state_machine::MechanicalRun> AgreedMechanicalRun(const state_machine::Calibrating& calibrating) const;

    private:
        static constexpr std::chrono::milliseconds saveRetryDelay{ 10 };
        static constexpr uint8_t maxMechanicalAttempts{ state_machine::Calibrating::maxMechanicalRuns };

        const LifecycleEnvironment& env;
        uint8_t agreeingRuns;
        CalibrationOrchestrator orchestrator;
        infra::Function<void(services::NvmStatus)> saveCompletion;
        services::CalibrationData pendingSaveData;
        infra::TimerSingleShot saveRetryTimer;
    };
}
