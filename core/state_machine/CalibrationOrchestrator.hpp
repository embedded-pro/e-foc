#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"

namespace application
{
    class CalibrationOrchestrator
    {
    public:
        CalibrationOrchestrator(services::ElectricalParametersIdentification& electricalIdent, services::MotorAlignment& motorAlignment, services::Tracer& tracer);

        void Start(
            services::CalibrationData& pendingData,
            const infra::Function<void(state_machine::CalibrationStep)>& onStepChanged,
            const infra::Function<void(foc::Radians)>& onAlignmentDone,
            const infra::Function<void()>& onFailed);

        void StartAlignmentOnly(
            services::CalibrationData& pendingData,
            const infra::Function<void(state_machine::CalibrationStep)>& onStepChanged,
            const infra::Function<void(foc::Radians)>& onAlignmentDone,
            const infra::Function<void()>& onFailed);

        void Abort();
        bool IsRunning() const;

    private:
        void RunPolePairsStep();
        void RunResistanceAndInductanceStep();
        void RunAlignmentStep();
        void Fail();

        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;
        services::Tracer& tracer;

        services::CalibrationData* pendingData{ nullptr };
        infra::Function<void(state_machine::CalibrationStep)> onStepChanged;
        infra::AutoResetFunction<void(foc::Radians)> onAlignmentDone;
        infra::AutoResetFunction<void()> onFailed;
        bool aborted{ false };
        uint32_t runToken{ 0 };
    };
}
