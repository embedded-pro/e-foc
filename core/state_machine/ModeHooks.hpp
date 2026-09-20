#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace application
{
    class ModeHooks
    {
    public:
        virtual ~ModeHooks() = default;

        virtual foc::FocBase& GetFoc() = 0;
        virtual foc::Controllable& GetFocControl() = 0;
        virtual foc::CurrentLoopTunable& CurrentTunable() = 0;
        virtual void RunPostAlignmentStep(state_machine::Calibrating& calibrating) = 0;

        // Mechanical identification runs the drive under a provisional plant. Whatever ends the run has to
        // say what becomes of it: a committed calibration supersedes it, anything else must put back the
        // model the drive held before the run.
        virtual void ProvisionalControlSuperseded() = 0;
        virtual void RestoreControlAfterProvisionalIdentification() = 0;

        virtual void ApplyModeSpecificCalibration(const services::CalibrationData& data) = 0;
        virtual bool HasValidModeSpecificCalibration(const services::CalibrationData& data) const = 0;
        virtual void PrepareForEnabled() = 0;
        virtual void AbortModeSpecificServices() = 0;
        virtual bool HasModeSpecificWorkPending() const = 0;
        virtual void RegisterModeSpecificCli(services::TerminalWithStorage& terminal) = 0;
    };
}
