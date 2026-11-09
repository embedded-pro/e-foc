#include "core/state_machine/DefaultModeHooks.hpp"

namespace application
{
    bool DefaultModeHooks::RunPostMechanicalStep(state_machine::Calibrating&)
    {
        return false;
    }

    void DefaultModeHooks::ApplyModeSpecificCalibration(const services::CalibrationData&)
    {}

    bool DefaultModeHooks::HasValidModeSpecificCalibration(const services::CalibrationData&) const
    {
        return true;
    }

    void DefaultModeHooks::PrepareForEnabled()
    {}

    void DefaultModeHooks::AbortModeSpecificServices()
    {}

    bool DefaultModeHooks::HasModeSpecificWorkPending() const
    {
        return false;
    }
}
