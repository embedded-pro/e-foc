#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-6f;
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

    class TestSpeedPlantModel
        : public ::testing::Test
    {
    };
}

TEST_F(TestSpeedPlantModel, a_complete_mechanical_set_is_valid)
{
    EXPECT_TRUE(foc::AreMechanicalParametersValid(ValidParameters()));
}

TEST_F(TestSpeedPlantModel, a_default_constructed_set_is_rejected)
{
    EXPECT_FALSE(foc::AreMechanicalParametersValid(foc::MechanicalModelParameters{}));
}

TEST_F(TestSpeedPlantModel, a_non_positive_inertia_is_rejected)
{
    auto parameters = ValidParameters();

    parameters.inertia = foc::NewtonMeterSecondSquared{ 0.0f };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));

    parameters.inertia = foc::NewtonMeterSecondSquared{ -inertia };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));
}

TEST_F(TestSpeedPlantModel, a_frictionless_load_is_accepted_but_a_negative_friction_is_not)
{
    auto parameters = ValidParameters();

    parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0f };
    EXPECT_TRUE(foc::AreMechanicalParametersValid(parameters));

    parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ -viscousFriction };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));
}

TEST_F(TestSpeedPlantModel, a_non_positive_torque_constant_is_rejected)
{
    auto parameters = ValidParameters();

    parameters.torqueConstant = foc::NewtonMeter{ 0.0f };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));

    parameters.torqueConstant = foc::NewtonMeter{ -torqueConstant };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));
}

TEST_F(TestSpeedPlantModel, a_non_positive_current_envelope_is_rejected)
{
    auto parameters = ValidParameters();

    parameters.maxCurrent = foc::Ampere{ 0.0f };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));

    parameters.maxCurrent = foc::Ampere{ -maxCurrent };
    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));
}

TEST_F(TestSpeedPlantModel, a_zero_sampling_frequency_is_rejected)
{
    auto parameters = ValidParameters();
    parameters.samplingFrequency = hal::Hertz{ 0 };

    EXPECT_FALSE(foc::AreMechanicalParametersValid(parameters));
}

TEST_F(TestSpeedPlantModel, the_outer_sample_period_is_the_reciprocal_of_the_sampling_frequency)
{
    EXPECT_NEAR(foc::OuterSamplePeriod(hal::Hertz{ samplingFrequency }), samplePeriod, tolerance);
    EXPECT_NEAR(foc::OuterSamplePeriod(hal::Hertz{ 20000 }), 5.0e-5f, tolerance);
}

TEST_F(TestSpeedPlantModel, the_plant_input_gain_is_the_torque_constant_over_the_inertia)
{
    EXPECT_NEAR(foc::PlantInputGain(ValidParameters()), torqueConstant / inertia, 1e-3f);
}

TEST_F(TestSpeedPlantModel, the_discretisation_decays_the_speed_by_the_friction_over_one_sample)
{
    const auto model = foc::SpeedPlantModel::FromParameters(ValidParameters());

    EXPECT_NEAR(model.ad, 1.0f - viscousFriction * samplePeriod / inertia, tolerance);
    EXPECT_NEAR(model.bd, torqueConstant / inertia * samplePeriod, tolerance);
}

TEST_F(TestSpeedPlantModel, a_frictionless_load_discretises_to_a_pure_integrator)
{
    auto parameters = ValidParameters();
    parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0f };

    EXPECT_NEAR(foc::SpeedPlantModel::FromParameters(parameters).ad, 1.0f, tolerance);
}

TEST_F(TestSpeedPlantModel, a_faster_sampling_rate_moves_the_pole_towards_unity)
{
    auto parameters = ValidParameters();
    parameters.samplingFrequency = hal::Hertz{ 10 * samplingFrequency };

    EXPECT_GT(foc::SpeedPlantModel::FromParameters(parameters).ad, foc::SpeedPlantModel::FromParameters(ValidParameters()).ad);
}

TEST_F(TestSpeedPlantModel, a_current_inside_the_envelope_passes_through_unchanged)
{
    EXPECT_NEAR(foc::LimitToCurrentEnvelope(3.0f, foc::Ampere{ maxCurrent }).Value(), 3.0f, tolerance);
    EXPECT_NEAR(foc::LimitToCurrentEnvelope(-3.0f, foc::Ampere{ maxCurrent }).Value(), -3.0f, tolerance);
}

TEST_F(TestSpeedPlantModel, a_current_beyond_the_envelope_is_clamped_on_both_sides)
{
    EXPECT_NEAR(foc::LimitToCurrentEnvelope(100.0f, foc::Ampere{ maxCurrent }).Value(), maxCurrent, tolerance);
    EXPECT_NEAR(foc::LimitToCurrentEnvelope(-100.0f, foc::Ampere{ maxCurrent }).Value(), -maxCurrent, tolerance);
}
