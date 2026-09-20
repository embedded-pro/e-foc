#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "core/state_machine/ModeHooks.hpp"
#include "core/state_machine/PendingCommand.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    class MaintenanceFlow
    {
    public:
        MaintenanceFlow(LifecycleMachine& machine,
            CalibrationContext& context,
            services::NonVolatileMemory& nvm,
            PendingCommand& pending,
            ModeHooks& mode,
            services::Tracer& tracer,
            const CalibrationFlow& calibration);

        void BeginClear(const state_machine::ClearCalibration& command);
        state_machine::Idle CompleteClear();
        void RejectClear();

        bool IsAcceptableFluxLinkage(const state_machine::SetFluxLinkage& command) const;
        void BeginSetFluxLinkage(const state_machine::SetFluxLinkage& command);
        void OnFluxLinkageSaved(services::NvmStatus status);

    private:
        LifecycleMachine& machine;
        CalibrationContext& context;
        services::NonVolatileMemory& nvm;
        PendingCommand& pending;
        ModeHooks& mode;
        services::Tracer& tracer;
        const CalibrationFlow& calibration;
    };
}
