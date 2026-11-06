#include "core/state_machine/Recalibration.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace
{
    constexpr float resistance{ 0.36f };
    constexpr float inductance{ 0.2f };
    constexpr float inertia{ 1.0e-5f };
    constexpr float friction{ 4.0e-6f };

    state_machine::MotorModel Commissioned()
    {
        return state_machine::MotorModel{ foc::Ohm{ resistance }, foc::MilliHenry{ inductance }, foc::NewtonMeterSecondSquared{ inertia }, foc::NewtonMeterSecondPerRadian{ friction } };
    }

    state_machine::OnlineEstimates Estimates(float resistanceRatio, float inductanceRatio, float inertiaRatio, float frictionRatio, bool settled = true)
    {
        return state_machine::OnlineEstimates{ foc::Ohm{ resistance * resistanceRatio }, foc::MilliHenry{ inductance * inductanceRatio },
            foc::NewtonMeterSecondSquared{ inertia * inertiaRatio }, foc::NewtonMeterSecondPerRadian{ friction * frictionRatio }, settled, settled };
    }

    class TestRecalibration
        : public testing::Test
    {
    public:
        TestRecalibration()
        {
            recalibration.Commission(Commissioned());
        }

        void Run(const state_machine::OnlineEstimates& estimates, infra::Duration duration)
        {
            for (auto elapsed = infra::Duration{}; elapsed < duration; elapsed += state_machine::Recalibration::observationPeriod)
                recalibration.Observe(estimates);
        }

        float InertiaInUse() const
        {
            return recalibration.InUse().inertia.Value();
        }

        state_machine::Recalibration recalibration;
    };
}

TEST_F(TestRecalibration, the_model_in_use_starts_as_the_commissioned_one)
{
    EXPECT_EQ(Commissioned(), recalibration.InUse());
    EXPECT_EQ(0, recalibration.Applied());
    EXPECT_EQ(0, recalibration.Rejected());
}

TEST_F(TestRecalibration, an_estimate_off_by_more_than_the_threshold_is_adopted_after_the_dwell)
{
    Run(Estimates(1.0f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell - state_machine::Recalibration::observationPeriod);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);
    EXPECT_NEAR(inertia, InertiaInUse(), 1e-12f);

    Run(Estimates(1.0f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell);
    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    EXPECT_NEAR(1.5f * inertia, InertiaInUse(), 1e-12f);
    EXPECT_EQ(1, outcome.applied);
    EXPECT_EQ(1, recalibration.Applied());
    EXPECT_NEAR(resistance, recalibration.InUse().resistance.Value(), 1e-9f);
}

TEST_F(TestRecalibration, an_estimate_within_the_threshold_is_not_adopted)
{
    Run(Estimates(1.08f, 0.93f, 1.09f, 1.05f), 2 * state_machine::Recalibration::dwell);

    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::command);

    EXPECT_EQ(Commissioned(), recalibration.InUse());
    EXPECT_EQ(0, outcome.applied);
}

TEST_F(TestRecalibration, an_estimate_that_has_not_settled_is_never_adopted)
{
    Run(Estimates(1.5f, 1.3f, 2.0f, 2.0f, false), 2 * state_machine::Recalibration::dwell);

    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);
    recalibration.Decide(state_machine::RecalibrationTrigger::command);

    EXPECT_EQ(Commissioned(), recalibration.InUse());
}

TEST_F(TestRecalibration, a_reading_back_within_the_threshold_starts_the_dwell_again)
{
    Run(Estimates(1.0f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell - state_machine::Recalibration::observationPeriod);
    Run(Estimates(1.0f, 1.0f, 1.0f, 1.0f), state_machine::Recalibration::observationPeriod);
    Run(Estimates(1.0f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::observationPeriod);

    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    EXPECT_NEAR(inertia, InertiaInUse(), 1e-12f);
}

TEST_F(TestRecalibration, a_command_adopts_a_settled_estimate_without_the_dwell)
{
    Run(Estimates(1.3f, 1.0f, 1.0f, 1.0f), state_machine::Recalibration::observationPeriod);

    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::command);

    EXPECT_NEAR(1.3f * resistance, recalibration.InUse().resistance.Value(), 1e-6f);
    EXPECT_EQ(1, outcome.applied);
}

TEST_F(TestRecalibration, an_estimate_outside_its_band_is_rejected_and_counted)
{
    Run(Estimates(2.5f, 0.4f, 6.0f, 0.1f), state_machine::Recalibration::dwell);

    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    EXPECT_EQ(Commissioned(), recalibration.InUse());
    EXPECT_EQ(0, outcome.applied);
    EXPECT_EQ(4, outcome.rejected);
    EXPECT_EQ(4, recalibration.Rejected());
}

TEST_F(TestRecalibration, the_bands_are_measured_from_the_commissioned_value)
{
    Run(Estimates(1.99f, 1.49f, 4.99f, 0.21f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);
    EXPECT_EQ(4, recalibration.Applied());

    Run(Estimates(0.71f, 0.51f, 0.51f, 4.99f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    EXPECT_EQ(8, recalibration.Applied());
    EXPECT_NEAR(0.51f * inertia, InertiaInUse(), 1e-12f);
}

TEST_F(TestRecalibration, the_deviation_is_measured_from_the_model_in_use)
{
    Run(Estimates(1.0f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    Run(Estimates(1.0f, 1.0f, 1.55f, 1.0f), 2 * state_machine::Recalibration::dwell);
    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    EXPECT_EQ(0, outcome.applied);
    EXPECT_NEAR(1.5f * inertia, InertiaInUse(), 1e-12f);
}

TEST_F(TestRecalibration, a_quantity_commissioned_without_a_value_is_never_recalibrated)
{
    auto withoutFriction = Commissioned();
    withoutFriction.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0f };
    recalibration.Commission(withoutFriction);

    Run(Estimates(1.0f, 1.0f, 1.0f, 2.0f), state_machine::Recalibration::dwell);
    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::command);

    EXPECT_EQ(0, outcome.applied);
    EXPECT_EQ(0, outcome.rejected);
    EXPECT_EQ(0.0f, recalibration.InUse().viscousFriction.Value());
}

TEST_F(TestRecalibration, a_new_calibration_record_replaces_the_model_in_use_and_the_same_record_keeps_it)
{
    Run(Estimates(1.0f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    recalibration.Commission(Commissioned());
    EXPECT_NEAR(1.5f * inertia, InertiaInUse(), 1e-12f);

    auto recalibrated = Commissioned();
    recalibrated.inertia = foc::NewtonMeterSecondSquared{ 3.0f * inertia };
    recalibration.Commission(recalibrated);
    EXPECT_EQ(recalibrated, recalibration.InUse());
}

TEST_F(TestRecalibration, a_non_finite_estimate_is_ignored)
{
    auto estimates = Estimates(1.0f, 1.0f, 1.5f, 1.0f);
    estimates.inertia = foc::NewtonMeterSecondSquared{ std::numeric_limits<float>::quiet_NaN() };
    Run(estimates, state_machine::Recalibration::dwell);

    const auto outcome = recalibration.Decide(state_machine::RecalibrationTrigger::command);

    EXPECT_EQ(0, outcome.applied);
    EXPECT_EQ(0, outcome.rejected);
}

TEST_F(TestRecalibration, a_stored_model_is_in_use_from_its_commissioning)
{
    state_machine::Recalibration restoredRecalibration;
    const state_machine::StoredModel stored{ foc::MilliHenry{ 1.2f * inductance }, foc::NewtonMeterSecondSquared{ 2.0f * inertia }, std::nullopt };

    EXPECT_TRUE(restoredRecalibration.Commission(Commissioned(), stored));

    EXPECT_NEAR(resistance, restoredRecalibration.InUse().resistance.Value(), 1e-9f);
    EXPECT_NEAR(1.2f * inductance, restoredRecalibration.InUse().inductance.Value(), 1e-9f);
    EXPECT_NEAR(2.0f * inertia, restoredRecalibration.InUse().inertia.Value(), 1e-12f);
    EXPECT_NEAR(friction, restoredRecalibration.InUse().viscousFriction.Value(), 1e-12f);
    EXPECT_EQ(0, restoredRecalibration.Applied());
}

TEST_F(TestRecalibration, a_stored_quantity_outside_its_band_is_not_used)
{
    state_machine::Recalibration restoredRecalibration;
    restoredRecalibration.Commission(Commissioned(), state_machine::StoredModel{ foc::MilliHenry{ 1.6f * inductance }, foc::NewtonMeterSecondSquared{ 6.0f * inertia }, foc::NewtonMeterSecondPerRadian{ -friction } });

    EXPECT_EQ(Commissioned(), restoredRecalibration.InUse());
}

TEST_F(TestRecalibration, commissioning_again_with_the_same_models_keeps_the_recalibrated_one)
{
    Run(Estimates(1.4f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    EXPECT_FALSE(recalibration.Commission(Commissioned()));

    EXPECT_NEAR(1.4f * resistance, recalibration.InUse().resistance.Value(), 1e-6f);
    EXPECT_NEAR(1.5f * inertia, InertiaInUse(), 1e-12f);
}

TEST_F(TestRecalibration, a_stored_model_that_is_discarded_returns_the_model_to_the_commissioned_one)
{
    state_machine::Recalibration restoredRecalibration;
    restoredRecalibration.Commission(Commissioned(), state_machine::StoredModel{ std::nullopt, foc::NewtonMeterSecondSquared{ 2.0f * inertia }, std::nullopt });

    EXPECT_TRUE(restoredRecalibration.Commission(Commissioned(), state_machine::StoredModel{}));

    EXPECT_EQ(Commissioned(), restoredRecalibration.InUse());
}

TEST_F(TestRecalibration, the_learned_model_holds_the_inductance_inertia_and_friction_that_differ_from_the_commissioned_ones)
{
    EXPECT_EQ(state_machine::StoredModel{}, recalibration.Learned());

    Run(Estimates(1.4f, 1.0f, 1.5f, 1.0f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    const auto learned = recalibration.Learned();
    EXPECT_FALSE(learned.inductance.has_value());
    ASSERT_TRUE(learned.inertia.has_value());
    EXPECT_NEAR(1.5f * inertia, learned.inertia->Value(), 1e-12f);
    EXPECT_FALSE(learned.viscousFriction.has_value());
}

TEST_F(TestRecalibration, reverting_returns_the_model_in_use_to_the_commissioned_one)
{
    recalibration.Commission(Commissioned(), state_machine::StoredModel{ std::nullopt, foc::NewtonMeterSecondSquared{ 2.0f * inertia }, std::nullopt });
    Run(Estimates(1.4f, 1.0f, 2.0f, 1.0f), state_machine::Recalibration::dwell);
    recalibration.Decide(state_machine::RecalibrationTrigger::automatic);

    recalibration.Revert();

    EXPECT_EQ(Commissioned(), recalibration.InUse());
    EXPECT_FALSE(recalibration.Commission(Commissioned()));
}

TEST_F(TestRecalibration, it_tells_whether_it_was_commissioned_from_a_record_and_a_stored_model)
{
    const state_machine::StoredModel stored{ std::nullopt, foc::NewtonMeterSecondSquared{ 2.0f * inertia }, std::nullopt };

    EXPECT_TRUE(recalibration.IsCommissionedFrom(Commissioned(), {}));
    EXPECT_FALSE(recalibration.IsCommissionedFrom(Commissioned(), stored));
    EXPECT_FALSE(state_machine::Recalibration{}.IsCommissionedFrom(Commissioned(), {}));
}
