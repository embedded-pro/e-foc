#pragma once

#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/IndexReference.hpp"
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
        bool ResumesAligned(services::NvmStatus status) const;
        state_machine::Ready FinishAligned();
        bool ResumesFromIndex(services::NvmStatus status, const IndexReference& index) const;
        state_machine::Ready FinishFromIndex();

    private:
        void ApplyRecord();
        void OnValidityChecked(bool valid);
        void OnCalibrationLoaded(services::NvmStatus status);

    private:
        const LifecycleEnvironment& env;
        const CalibrationFlow& calibration;
    };
}
