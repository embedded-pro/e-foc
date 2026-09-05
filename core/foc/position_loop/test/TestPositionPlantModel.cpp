#include "core/foc/position_loop/PositionPlantModel.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float tolerance = 1e-5f;
    constexpr float inertia = 1.0e-4f;
    constexpr float viscousFriction = 1.0e-4f;
    constexpr float torqueConstant = 0.05f;
    constexpr float maxCurrent = 10.0f;
    constexpr uint32_t samplingFrequency = 1000;
    constexpr float samplePeriod = 1.0f / static_cast<float>(samplingFrequency);

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ inertia }, foc::NewtonMeterSecondPerRadian{ viscousFriction },
            foc::NewtonMeter{ torqueConstant }, foc::Ampere{ maxCurrent }, hal::Hertz{ samplingFrequency } };
    }

    class TestPositionPlantModel
        : public ::testing::Test
    {
    };
}

TEST_F(TestPositionPlantModel, the_scaled_speed_state_decays_with_the_viscous_friction)
{
    const auto model = foc::PositionPlantModel::FromParameters(ValidParameters());

    EXPECT_NEAR(model.speedDecay, 1.0f - viscousFriction * samplePeriod / inertia, tolerance);
}

TEST_F(TestPositionPlantModel, a_frictionless_load_leaves_the_scaled_speed_undamped)
{
    auto parameters = ValidParameters();
    parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0f };

    EXPECT_NEAR(foc::PositionPlantModel::FromParameters(parameters).speedDecay, 1.0f, tolerance);
}

TEST_F(TestPositionPlantModel, the_normalised_input_converts_back_to_amperes_through_the_rigid_body)
{
    const auto model = foc::PositionPlantModel::FromParameters(ValidParameters());

    EXPECT_NEAR(model.currentPerNormalizedInput, inertia / (torqueConstant * samplePeriod * samplePeriod), 1e-2f);
}

TEST_F(TestPositionPlantModel, a_zero_torque_constant_leaves_the_input_conversion_at_zero)
{
    auto parameters = ValidParameters();
    parameters.torqueConstant = foc::NewtonMeter{ 0.0f };

    EXPECT_NEAR(foc::PositionPlantModel::FromParameters(parameters).currentPerNormalizedInput, 0.0f, tolerance);
}

TEST_F(TestPositionPlantModel, a_zero_current_envelope_leaves_the_input_conversion_at_zero)
{
    auto parameters = ValidParameters();
    parameters.maxCurrent = foc::Ampere{ 0.0f };

    EXPECT_NEAR(foc::PositionPlantModel::FromParameters(parameters).currentPerNormalizedInput, 0.0f, tolerance);
}

TEST_F(TestPositionPlantModel, a_heavier_load_needs_more_current_per_unit_of_normalised_input)
{
    auto heavy = ValidParameters();
    heavy.inertia = foc::NewtonMeterSecondSquared{ 10.0f * inertia };

    EXPECT_GT(foc::PositionPlantModel::FromParameters(heavy).currentPerNormalizedInput,
        foc::PositionPlantModel::FromParameters(ValidParameters()).currentPerNormalizedInput);
}

TEST_F(TestPositionPlantModel, the_effort_weight_is_the_inverse_square_of_the_bandwidth_per_sample)
{
    const float bandwidth = 100.0f;
    const float bandwidthPerSample = bandwidth * samplePeriod;

    EXPECT_NEAR(foc::NormalizedEffortWeight(bandwidth, hal::Hertz{ samplingFrequency }),
        1.0f / (bandwidthPerSample * bandwidthPerSample), 1e-3f);
}

TEST_F(TestPositionPlantModel, a_bandwidth_beyond_half_the_sampling_rate_is_clamped)
{
    EXPECT_NEAR(foc::NormalizedEffortWeight(1.0e6f, hal::Hertz{ samplingFrequency }), 4.0f, tolerance);
}

TEST_F(TestPositionPlantModel, a_vanishing_bandwidth_is_clamped_to_a_finite_effort_weight)
{
    EXPECT_NEAR(foc::NormalizedEffortWeight(1.0e-6f, hal::Hertz{ samplingFrequency }), 1.0e6f, 1.0f);
}

TEST_F(TestPositionPlantModel, a_lower_bandwidth_asks_for_a_higher_effort_weight)
{
    EXPECT_GT(foc::NormalizedEffortWeight(10.0f, hal::Hertz{ samplingFrequency }),
        foc::NormalizedEffortWeight(100.0f, hal::Hertz{ samplingFrequency }));
}

TEST_F(TestPositionPlantModel, a_weight_is_expressed_relative_to_the_position_anchor)
{
    EXPECT_NEAR(foc::WeightRatio(0.5f, 2.0f), 0.25f, tolerance);
    EXPECT_NEAR(foc::WeightRatio(2.0f, 1.0f), 2.0f, tolerance);
}

TEST_F(TestPositionPlantModel, a_non_positive_anchor_falls_back_to_unity)
{
    EXPECT_NEAR(foc::WeightRatio(0.5f, 0.0f), 0.5f, tolerance);
    EXPECT_NEAR(foc::WeightRatio(0.5f, -2.0f), 0.5f, tolerance);
}

TEST_F(TestPositionPlantModel, the_weight_ratio_is_clamped_on_both_ends)
{
    EXPECT_NEAR(foc::WeightRatio(1.0e9f, 1.0f), 1.0e3f, tolerance);
    EXPECT_NEAR(foc::WeightRatio(1.0e-9f, 1.0f), 1.0e-3f, tolerance);
}

TEST_F(TestPositionPlantModel, an_error_inside_the_principal_range_is_the_plain_difference)
{
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ 0.5f }, foc::Radians{ 0.2f }), 0.3f, tolerance);
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ 0.2f }, foc::Radians{ 0.5f }), -0.3f, tolerance);
}

TEST_F(TestPositionPlantModel, a_reference_just_past_the_seam_is_reached_by_moving_forward)
{
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ -pi + 0.1f }, foc::Radians{ pi - 0.1f }), 0.2f, 1e-4f);
}

TEST_F(TestPositionPlantModel, a_reference_just_before_the_seam_is_reached_by_moving_backward)
{
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ pi - 0.1f }, foc::Radians{ -pi + 0.1f }), -0.2f, 1e-4f);
}

TEST_F(TestPositionPlantModel, half_a_turn_of_error_sits_exactly_on_the_boundary_and_is_not_wrapped)
{
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ pi }, foc::Radians{ 0.0f }), pi, tolerance);
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ 0.0f }, foc::Radians{ pi }), -pi, tolerance);
}

TEST_F(TestPositionPlantModel, a_reference_several_turns_away_still_takes_the_shortest_path)
{
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ 10.0f }, foc::Radians{ 0.5f }), -3.0664f, 1e-3f);
    EXPECT_NEAR(foc::WrappedPositionError(foc::Radians{ -10.0f }, foc::Radians{ -0.5f }), 3.0664f, 1e-3f);
}

TEST_F(TestPositionPlantModel, every_wrapped_error_lands_inside_half_a_turn)
{
    constexpr float step = 0.37f;

    for (float reference = -40.0f; reference <= 40.0f; reference += step)
        EXPECT_LE(std::abs(foc::WrappedPositionError(foc::Radians{ reference }, foc::Radians{ 0.0f })), pi + 1e-4f);
}
