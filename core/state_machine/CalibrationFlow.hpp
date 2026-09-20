#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/CalibrationOrchestrator.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "core/state_machine/ModeHooks.hpp"
#include "core/state_machine/PendingCommand.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    class CalibrationFlow
    {
    public:
        CalibrationFlow(LifecycleMachine& machine,
            CalibrationContext& context,
            services::NonVolatileMemory& nvm,
            PendingCommand& pending,
            ModeHooks& mode,
            services::Tracer& tracer,
            services::ElectricalParametersIdentification& electricalIdent,
            services::MotorAlignment& motorAlignment);

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
        void Abort();

    private:
        LifecycleMachine& machine;
        CalibrationContext& context;
        services::NonVolatileMemory& nvm;
        PendingCommand& pending;
        ModeHooks& mode;
        services::Tracer& tracer;
        CalibrationOrchestrator orchestrator;
    };
}
