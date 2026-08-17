#include "core/foc/position_loop/PositionControllerSelector.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    foc::MechanicalModelParameters ValidParameters()
    {
        auto parameters = foc::MechanicalModelParameters{};
        parameters.inertia = foc::NewtonMeterSecondSquared{ 0.001f };
        parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0001f };
        parameters.torqueConstant = foc::NewtonMeter{ 0.1f };
        parameters.maxCurrent = foc::Ampere{ 10.0f };
        parameters.samplingFrequency = hal::Hertz{ 1000 };
        return parameters;
    }

    foc::PositionControlContext Context(float reference, float measured, float speed)
    {
        return { foc::Radians{ measured }, foc::Radians{ reference }, foc::RadiansPerSecond{ speed } };
    }

    class TestPositionControllerSelector
        : public ::testing::Test
    {
    public:
        foc::PositionControllerSelector selector;
    };
}

TEST_F(TestPositionControllerSelector, pid_is_active_by_default)
{
    EXPECT_EQ(selector.Active(), foc::PositionAlgorithm::pid);
}

TEST_F(TestPositionControllerSelector, bandwidth_only_algorithms_need_no_mechanical_model)
{
    EXPECT_EQ(selector.Select(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);
    EXPECT_EQ(selector.Active(), foc::PositionAlgorithm::cascadeP);
}

TEST_F(TestPositionControllerSelector, state_feedback_algorithms_require_a_mechanical_model)
{
    EXPECT_EQ(selector.Select(foc::PositionAlgorithm::lqr), foc::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Select(foc::PositionAlgorithm::lqi), foc::SelectResult::invalidParameters);
}

TEST_F(TestPositionControllerSelector, every_algorithm_can_be_selected_with_valid_parameters)
{
    selector.Configure(ValidParameters());

    for (auto algorithm : { foc::PositionAlgorithm::pid,
             foc::PositionAlgorithm::cascadeP,
             foc::PositionAlgorithm::lqr,
             foc::PositionAlgorithm::lqi,
             foc::PositionAlgorithm::twoDof })
    {
        EXPECT_EQ(selector.Select(algorithm), foc::SelectResult::ok);
        EXPECT_EQ(selector.Active(), algorithm);
    }
}

TEST_F(TestPositionControllerSelector, unknown_algorithm_is_rejected)
{
    EXPECT_EQ(selector.Select(static_cast<foc::PositionAlgorithm>(200)), foc::SelectResult::invalidAlgorithm);
}

TEST_F(TestPositionControllerSelector, cascade_p_drives_the_speed_loop)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);

    const auto output = selector.Compute(Context(1.0f, 0.0f, 0.0f));

    EXPECT_EQ(output.kind, foc::PositionOutputKind::speedReference);
    EXPECT_GT(output.value, 0.0f);
}

TEST_F(TestPositionControllerSelector, state_feedback_commands_current_directly)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::lqr), foc::SelectResult::ok);

    const auto output = selector.Compute(Context(1.0f, 0.0f, 0.0f));

    EXPECT_EQ(output.kind, foc::PositionOutputKind::currentReference);
    EXPECT_GT(output.value, 0.0f);
    EXPECT_LE(output.value, ValidParameters().maxCurrent.Value());
}

TEST_F(TestPositionControllerSelector, zero_error_produces_zero_command)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);

    EXPECT_FLOAT_EQ(selector.Compute(Context(0.5f, 0.5f, 0.0f)).value, 0.0f);
}

TEST_F(TestPositionControllerSelector, a_rejected_design_leaves_the_previous_algorithm_running)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);

    // Zeroing the torque constant makes the rigid-body model unusable for state feedback
    auto degraded = ValidParameters();
    degraded.torqueConstant = foc::NewtonMeter{ 0.0f };
    selector.Configure(degraded);

    EXPECT_EQ(selector.Select(foc::PositionAlgorithm::lqi), foc::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Active(), foc::PositionAlgorithm::cascadeP);
}

TEST_F(TestPositionControllerSelector, tunings_the_active_algorithm_cannot_be_designed_for_are_refused)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::lqr), foc::SelectResult::ok);

    auto degraded = ValidParameters();
    degraded.torqueConstant = foc::NewtonMeter{ 0.0f };
    selector.Configure(degraded);

    EXPECT_EQ(selector.TrySetTunings(foc::PositionLoopTunings{}), foc::SelectResult::invalidParameters);
}

TEST_F(TestPositionControllerSelector, the_error_takes_the_short_way_across_the_wrap_seam)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);

    constexpr float pi = std::numbers::pi_v<float>;

    // Just past the seam is a 0.2 rad step forward, not a 6.08 rad run backwards
    const auto forward = selector.Compute(Context(-pi + 0.1f, pi - 0.1f, 0.0f)).value;
    const auto reference = selector.Compute(Context(0.2f, 0.0f, 0.0f)).value;

    EXPECT_GT(forward, 0.0f);
    EXPECT_NEAR(forward, reference, 1e-3f);
}

TEST_F(TestPositionControllerSelector, the_integral_law_keeps_pushing_on_a_standing_error)
{
    selector.Configure(ValidParameters());
    ASSERT_EQ(selector.Select(foc::PositionAlgorithm::lqi), foc::SelectResult::ok);

    const auto first = selector.Compute(Context(0.001f, 0.0f, 0.0f)).value;
    const auto second = selector.Compute(Context(0.001f, 0.0f, 0.0f)).value;

    EXPECT_GT(second, first);
}

TEST_F(TestPositionControllerSelector, every_algorithm_stays_within_the_current_envelope)
{
    selector.Configure(ValidParameters());

    for (auto algorithm : { foc::PositionAlgorithm::lqr, foc::PositionAlgorithm::lqi })
    {
        ASSERT_EQ(selector.Select(algorithm), foc::SelectResult::ok);

        for (int sample = 0; sample != 100; ++sample)
        {
            const auto output = selector.Compute(Context(3.0f, 0.0f, 0.0f));
            EXPECT_LE(std::abs(output.value), ValidParameters().maxCurrent.Value());
        }
    }
}
