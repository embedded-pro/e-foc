#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/LifecycleContext.hpp"

namespace application
{
    class MaintenanceFlow
    {
    public:
        MaintenanceFlow(const LifecycleEnvironment& environment, const CalibrationFlow& calibration);

        void BeginClear(const state_machine::ClearCalibration& command);
        state_machine::Idle CompleteClear();
        void ReconcileClear();
        void RejectClear();

        bool IsAcceptableFluxLinkage(const state_machine::SetFluxLinkage& command) const;
        void BeginSetFluxLinkage(const state_machine::SetFluxLinkage& command);
        void OnFluxLinkageSaved(services::NvmStatus status);

    private:
        void OnInvalidated(services::NvmStatus status);
        void OnFluxLinkageStored(services::NvmStatus status);

    private:
        LifecycleEnvironment env;
        const CalibrationFlow& calibration;
    };
}
