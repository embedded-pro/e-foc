#include "core/state_machine/DefaultModeHooks.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    class HooksWithDefaults
        : public application::DefaultModeHooks
    {
    public:
        MOCK_METHOD(foc::FocBase&, GetFoc, (), (override));
        MOCK_METHOD(foc::Controllable&, GetFocControl, (), (override));
        MOCK_METHOD(foc::CurrentLoopTunable&, CurrentTunable, (), (override));
        MOCK_METHOD(services::FluxLinkageIdentification*, FluxLinkageIdent, (), (override));
        MOCK_METHOD(bool, ApplyProvisionalCurrentControl, (const services::CalibrationData& pending), (override));
        MOCK_METHOD(void, RunPostAlignmentStep, (state_machine::Calibrating & calibrating), (override));
        MOCK_METHOD(void, ProvisionalControlSuperseded, (), (override));
        MOCK_METHOD(void, RestoreControlAfterProvisionalIdentification, (), (override));
    };
}

TEST(TestDefaultModeHooks, a_mode_without_its_own_steps_adds_nothing_to_calibration_or_enable)
{
    testing::StrictMock<HooksWithDefaults> hooks;
    state_machine::Calibrating calibrating{};
    const services::CalibrationData data{};

    EXPECT_FALSE(hooks.RunPostMechanicalStep(calibrating));
    hooks.ApplyModeSpecificCalibration(data);
    EXPECT_TRUE(hooks.HasValidModeSpecificCalibration(data));
    hooks.PrepareForEnabled();
    hooks.AbortModeSpecificServices();
    EXPECT_FALSE(hooks.HasModeSpecificWorkPending());
}
