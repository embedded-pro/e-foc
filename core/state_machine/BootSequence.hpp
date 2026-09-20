#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/LifecycleContext.hpp"

namespace application
{
    class BootSequence
    {
    public:
        BootSequence(const LifecycleEnvironment& environment, const CalibrationFlow& calibration);

        void Begin();
        void Continue(bool valid);
        void Finish(services::NvmStatus status);

    private:
        void OnValidityChecked(bool valid);
        void OnCalibrationLoaded(services::NvmStatus status);

    private:
        const LifecycleEnvironment& env;
        const CalibrationFlow& calibration;
    };
}
