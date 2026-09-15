#include "TestFocStateMachineHelper.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/state_machine/CalibrationContext.hpp"

namespace
{
    using namespace testing;

    services::CalibrationData CompleteData()
    {
        services::CalibrationData d{};
        d.stage = services::CalibrationStage::complete;
        d.polePairs = 7;
        d.rPhase = 0.5f;
        d.lD = 1.0f;
        d.fluxLinkage = 0.007f;
        d.currentLoopBandwidth = 1000.0f;
        return d;
    }

    class CalibrationContextTest
        : public ::testing::Test
    {
    public:
        StrictMock<drivers::ThreePhaseInverterMock> inverter;
        application::CalibrationContext context{ inverter, foc::Volts{ 24.0f }, foc::Weber{ 0.005f } };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverter, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverter, Stop()).Times(AnyNumber());
            } };
    };
}

TEST_F(CalibrationContextTest, default_data_is_not_complete)
{
    EXPECT_FALSE(context.IsComplete(true));
}

TEST_F(CalibrationContextTest, is_complete_requires_complete_stage)
{
    auto d = CompleteData();
    d.stage = services::CalibrationStage::none;
    context.SetData(d);
    EXPECT_FALSE(context.IsComplete(true));
}

TEST_F(CalibrationContextTest, is_complete_requires_nonzero_pole_pairs)
{
    auto d = CompleteData();
    d.polePairs = 0;
    context.SetData(d);
    EXPECT_FALSE(context.IsComplete(true));
}

TEST_F(CalibrationContextTest, is_complete_false_when_mode_specific_invalid)
{
    context.SetData(CompleteData());
    EXPECT_FALSE(context.IsComplete(false));
}

TEST_F(CalibrationContextTest, is_complete_true_with_valid_data_and_mode_check)
{
    context.SetData(CompleteData());
    EXPECT_TRUE(context.IsComplete(true));
}

TEST_F(CalibrationContextTest, has_partial_true_when_poles_nonzero_and_stage_not_complete)
{
    services::CalibrationData d{};
    d.polePairs = 4;
    context.SetData(d);
    EXPECT_TRUE(context.HasPartial());
}

TEST_F(CalibrationContextTest, has_partial_false_when_complete)
{
    context.SetData(CompleteData());
    EXPECT_FALSE(context.HasPartial());
}

TEST_F(CalibrationContextTest, has_partial_false_when_empty)
{
    EXPECT_FALSE(context.HasPartial());
}

TEST_F(CalibrationContextTest, rotor_reference_valid_false_by_default)
{
    EXPECT_FALSE(context.IsRotorReferenceValid());
}

TEST_F(CalibrationContextTest, rotor_reference_valid_set_get)
{
    context.SetRotorReferenceValid(true);
    EXPECT_TRUE(context.IsRotorReferenceValid());
    context.SetRotorReferenceValid(false);
    EXPECT_FALSE(context.IsRotorReferenceValid());
}

TEST_F(CalibrationContextTest, invalidate_resets_data_and_rotor_reference)
{
    context.SetData(CompleteData());
    context.SetRotorReferenceValid(true);

    context.Invalidate();

    EXPECT_FALSE(context.IsComplete(true));
    EXPECT_FALSE(context.IsRotorReferenceValid());
}

TEST_F(CalibrationContextTest, pending_flux_linkage_commit_updates_calibration_data)
{
    context.SetData(CompleteData());
    context.SetPendingFluxLinkage(0.010f);
    context.CommitPendingFluxLinkage();
    EXPECT_NEAR(context.Data().fluxLinkage, 0.010f, 1e-6f);
}

TEST_F(CalibrationContextTest, effective_flux_linkage_returns_calibrated_value_when_positive)
{
    context.SetData(CompleteData());
    EXPECT_NEAR(context.EffectiveFluxLinkage().Value(), 0.007f, 1e-6f);
}

TEST_F(CalibrationContextTest, effective_flux_linkage_returns_configured_default_when_not_positive)
{
    services::CalibrationData d = CompleteData();
    d.fluxLinkage = 0.0f;
    context.SetData(d);
    EXPECT_NEAR(context.EffectiveFluxLinkage().Value(), 0.005f, 1e-6f);
}

TEST_F(CalibrationContextTest, apply_calls_foc_configure_and_sets_current_tunings)
{
    context.SetData(CompleteData());
    StrictMock<foc::FocTorqueMock> foc;
    EXPECT_CALL(foc, Configure(_)).WillOnce(Return(true));
    EXPECT_CALL(foc, SetCurrentTunings(_));
    context.Apply(foc, foc);
}

TEST_F(CalibrationContextTest, default_current_loop_bandwidth_uses_nyquist_factor)
{
    const float expected = (10000.0f / 15.0f) * 2.0f * 3.14159265f;
    EXPECT_NEAR(context.DefaultCurrentLoopBandwidth(), expected, 1.0f);
}
