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

TEST_F(CalibrationContextTest, apply_gives_the_controller_the_board_pwm_update_delay)
{
    application::CalibrationContext lateDuty{ inverter, foc::Volts{ 24.0f }, foc::Weber{ 0.005f }, std::nullopt, 1 };
    lateDuty.SetData(CompleteData());
    StrictMock<foc::FocTorqueMock> foc;
    EXPECT_CALL(foc, Configure(_)).WillOnce(Invoke([](const foc::MotorModelParameters& parameters)
        {
            EXPECT_EQ(parameters.dutyUpdateDelayPeriods, 1u);
            return true;
        }));
    EXPECT_CALL(foc, SetCurrentTunings(_));

    lateDuty.Apply(foc, foc);
}

TEST_F(CalibrationContextTest, apply_pushes_the_stored_current_offsets_and_invalidate_clears_them)
{
    StrictMock<services::PhaseCurrentOffsetsMock> currentOffsets;
    application::CalibrationContext withOffsets{ inverter, foc::Volts{ 24.0f }, foc::Weber{ 0.005f }, std::ref(currentOffsets) };
    auto d = CompleteData();
    d.currentOffsetA = 0.027f;
    d.currentOffsetB = -0.018f;
    d.currentOffsetC = 0.054f;
    withOffsets.SetData(d);
    StrictMock<foc::FocTorqueMock> foc;
    EXPECT_CALL(foc, Configure(_)).WillOnce(Return(true));
    EXPECT_CALL(foc, SetCurrentTunings(_));

    EXPECT_CALL(currentOffsets, Apply(_)).WillOnce(Invoke([](const foc::PhaseCurrents& offsets)
        {
            EXPECT_NEAR(offsets.a.Value(), 0.027f, 1e-6f);
            EXPECT_NEAR(offsets.b.Value(), -0.018f, 1e-6f);
            EXPECT_NEAR(offsets.c.Value(), 0.054f, 1e-6f);
        }));
    withOffsets.Apply(foc, foc);

    EXPECT_CALL(currentOffsets, Apply(_)).WillOnce(Invoke([](const foc::PhaseCurrents& offsets)
        {
            EXPECT_NEAR(offsets.a.Value(), 0.0f, 1e-6f);
            EXPECT_NEAR(offsets.b.Value(), 0.0f, 1e-6f);
            EXPECT_NEAR(offsets.c.Value(), 0.0f, 1e-6f);
        }));
    withOffsets.Invalidate();
}

TEST_F(CalibrationContextTest, default_current_loop_bandwidth_uses_nyquist_factor)
{
    const float expected = (10000.0f / 15.0f) * 2.0f * 3.14159265f;
    EXPECT_NEAR(context.DefaultCurrentLoopBandwidth(foc::MilliHenry{ 1.094f }, foc::Ampere{ 0.0f }), expected, 1.0f);
}

TEST_F(CalibrationContextTest, default_current_loop_bandwidth_keeps_the_measured_noise_at_0_08_volt)
{
    const float expected = 0.08f / (1.094e-3f * 0.0386f);
    EXPECT_NEAR(context.DefaultCurrentLoopBandwidth(foc::MilliHenry{ 1.094f }, foc::Ampere{ 0.0386f }), expected, 1.0f);
}

TEST_F(CalibrationContextTest, default_current_loop_bandwidth_stays_ten_times_the_default_speed_loop_for_a_noisy_sensor)
{
    EXPECT_NEAR(context.DefaultCurrentLoopBandwidth(foc::MilliHenry{ 1.094f }, foc::Ampere{ 0.255f }), 10.0f * foc::SpeedLoopTunings{}.bandwidth, 1.0f);
}

TEST_F(CalibrationContextTest, default_current_loop_bandwidth_keeps_the_pwm_bound_for_a_quiet_sensor)
{
    const float expected = (10000.0f / 15.0f) * 2.0f * 3.14159265f;
    EXPECT_NEAR(context.DefaultCurrentLoopBandwidth(foc::MilliHenry{ 1.094f }, foc::Ampere{ 0.001f }), expected, 1.0f);
}

TEST_F(CalibrationContextTest, apply_bounds_the_automatic_current_bandwidth_with_the_stored_noise)
{
    auto data = CompleteData();
    data.lD = 1.094f;
    data.currentLoopBandwidth = 0.0f;
    data.currentNoiseMilliAmpere = 30;
    context.SetData(data);
    StrictMock<foc::FocTorqueMock> foc;
    EXPECT_CALL(foc, Configure(_)).WillOnce(Return(true));
    EXPECT_CALL(foc, SetCurrentTunings(_)).WillOnce(Invoke([](const foc::CurrentLoopTunings& tunings)
        {
            EXPECT_NEAR(tunings.bandwidth, 0.08f / (1.094e-3f * 0.030f), 1.0f);
        }));

    context.Apply(foc, foc);
}
