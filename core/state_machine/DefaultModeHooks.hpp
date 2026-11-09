#pragma once

#include "core/state_machine/ModeHooks.hpp"

namespace application
{
    class DefaultModeHooks
        : public ModeHooks
    {
    protected:
        ~DefaultModeHooks() = default;

    public:
        bool RunPostMechanicalStep(state_machine::Calibrating& calibrating) override;
        void ApplyModeSpecificCalibration(const services::CalibrationData& data) override;
        bool HasValidModeSpecificCalibration(const services::CalibrationData& data) const override;
        void PrepareForEnabled() override;
        void AbortModeSpecificServices() override;
        bool HasModeSpecificWorkPending() const override;
    };
}
