#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "core/state_machine/ModeHooks.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    class BootSequence
    {
    public:
        BootSequence(LifecycleMachine& machine,
            CalibrationContext& context,
            services::NonVolatileMemory& nvm,
            ModeHooks& mode,
            services::Tracer& tracer,
            const CalibrationFlow& calibration);

        void Begin();
        void Continue(bool valid);
        void Finish(services::NvmStatus status);
        bool InFlight() const;

    private:
        void OnValidityChecked(bool valid);
        void OnCalibrationLoaded(services::NvmStatus status);

    private:
        LifecycleMachine& machine;
        CalibrationContext& context;
        services::NonVolatileMemory& nvm;
        ModeHooks& mode;
        services::Tracer& tracer;
        const CalibrationFlow& calibration;
        bool inFlight{ false };
    };
}
