#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1e-6f;
    constexpr float resistance = 1.0f;
    constexpr float inductanceInMilliHenry = 0.5f;
    constexpr float inductanceInHenry = 0.0005f;
    constexpr float fluxLinkage = 0.01f;
    constexpr float busVoltage = 24.0f;
    constexpr uint32_t samplingFrequency = 20000;
    constexpr float samplePeriod = 1.0f / static_cast<float>(samplingFrequency);

    foc::MotorModelParameters ValidParameters()
    {
        return { foc::Ohm{ resistance }, foc::MilliHenry{ inductanceInMilliHenry }, foc::Weber{ fluxLinkage },
            foc::Volts{ busVoltage }, hal::Hertz{ samplingFrequency } };
    }

    float NormalizationScale()
    {
        return std::numbers::sqrt3_v<float> / busVoltage;
    }

    class TestCurrentPlantModel
        : public ::testing::Test
    {
    };
}

TEST_F(TestCurrentPlantModel, a_complete_electrical_set_is_valid)
{
    EXPECT_TRUE(foc::AreElectricalParametersValid(ValidParameters()));
}

TEST_F(TestCurrentPlantModel, a_default_constructed_set_is_rejected)
{
    EXPECT_FALSE(foc::AreElectricalParametersValid(foc::MotorModelParameters{}));
}

TEST_F(TestCurrentPlantModel, a_non_positive_resistance_is_rejected)
{
    auto parameters = ValidParameters();

    parameters.resistance = foc::Ohm{ 0.0f };
    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));

    parameters.resistance = foc::Ohm{ -resistance };
    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));
}

TEST_F(TestCurrentPlantModel, a_non_positive_inductance_is_rejected)
{
    auto parameters = ValidParameters();

    parameters.inductance = foc::MilliHenry{ 0.0f };
    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));

    parameters.inductance = foc::MilliHenry{ -inductanceInMilliHenry };
    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));
}

TEST_F(TestCurrentPlantModel, a_non_positive_bus_voltage_is_rejected)
{
    auto parameters = ValidParameters();

    parameters.busVoltage = foc::Volts{ 0.0f };
    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));

    parameters.busVoltage = foc::Volts{ -busVoltage };
    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));
}

TEST_F(TestCurrentPlantModel, a_zero_sampling_frequency_is_rejected)
{
    auto parameters = ValidParameters();
    parameters.samplingFrequency = hal::Hertz{ 0 };

    EXPECT_FALSE(foc::AreElectricalParametersValid(parameters));
}

TEST_F(TestCurrentPlantModel, the_flux_linkage_is_not_part_of_the_electrical_guard)
{
    auto parameters = ValidParameters();
    parameters.fluxLinkage = foc::Weber{ 0.0f };

    EXPECT_TRUE(foc::AreElectricalParametersValid(parameters));
}

TEST_F(TestCurrentPlantModel, the_normalisation_scale_maps_the_inscribed_circle_onto_the_bus_voltage)
{
    EXPECT_NEAR(foc::NormalizationScale(foc::Volts{ busVoltage }), NormalizationScale(), tolerance);
}

TEST_F(TestCurrentPlantModel, the_sample_period_is_the_reciprocal_of_the_sampling_frequency)
{
    EXPECT_NEAR(foc::SamplePeriod(hal::Hertz{ samplingFrequency }), samplePeriod, tolerance);
}

TEST_F(TestCurrentPlantModel, millihenry_converts_to_henry)
{
    EXPECT_NEAR(foc::InductanceInHenry(foc::MilliHenry{ inductanceInMilliHenry }), inductanceInHenry, tolerance);
}

TEST_F(TestCurrentPlantModel, the_discretisation_is_the_exact_zero_order_hold_of_the_rl_branch)
{
    const auto model = foc::CurrentPlantModel::FromParameters(ValidParameters());
    const float expectedAd = std::exp(-resistance * samplePeriod / inductanceInHenry);

    EXPECT_NEAR(model.ad, expectedAd, tolerance);
    EXPECT_NEAR(model.bd, (1.0f - expectedAd) / resistance, tolerance);
}

TEST_F(TestCurrentPlantModel, the_discrete_dc_gain_is_the_conductance_of_the_winding)
{
    const auto model = foc::CurrentPlantModel::FromParameters(ValidParameters());

    EXPECT_NEAR(model.bd / (1.0f - model.ad), 1.0f / resistance, 1e-4f);
}

TEST_F(TestCurrentPlantModel, a_larger_inductance_slows_the_electrical_pole)
{
    auto parameters = ValidParameters();
    parameters.inductance = foc::MilliHenry{ 10.0f * inductanceInMilliHenry };

    EXPECT_GT(foc::CurrentPlantModel::FromParameters(parameters).ad, foc::CurrentPlantModel::FromParameters(ValidParameters()).ad);
}

TEST_F(TestCurrentPlantModel, the_feedforward_cancels_the_cross_coupling_and_the_back_emf)
{
    foc::DecouplingFeedforward feedforward;
    feedforward.Configure(ValidParameters());

    const foc::CurrentControlContext context{ { 2.0f, 3.0f }, { 0.0f, 0.0f }, 1000.0f };
    const auto output = feedforward.Apply({ 0.1f, 0.2f }, context);

    const float couplingScale = inductanceInHenry * NormalizationScale();
    const float backEmfScale = fluxLinkage * NormalizationScale();

    EXPECT_NEAR(output.d, 0.1f - 1000.0f * couplingScale * 3.0f, 1e-5f);
    EXPECT_NEAR(output.q, 0.2f + 1000.0f * (couplingScale * 2.0f + backEmfScale), 1e-5f);
}

TEST_F(TestCurrentPlantModel, the_feedforward_adds_nothing_at_standstill)
{
    foc::DecouplingFeedforward feedforward;
    feedforward.Configure(ValidParameters());

    const auto output = feedforward.Apply({ 0.1f, 0.2f }, { { 2.0f, 3.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.1f, tolerance);
    EXPECT_NEAR(output.q, 0.2f, tolerance);
}

TEST_F(TestCurrentPlantModel, an_unconfigured_feedforward_passes_the_voltages_through)
{
    foc::DecouplingFeedforward feedforward;

    const auto output = feedforward.Apply({ 0.1f, 0.2f }, { { 2.0f, 3.0f }, { 0.0f, 0.0f }, 1000.0f });

    EXPECT_NEAR(output.d, 0.1f, tolerance);
    EXPECT_NEAR(output.q, 0.2f, tolerance);
}

TEST_F(TestCurrentPlantModel, invalid_parameters_disable_the_feedforward)
{
    auto parameters = ValidParameters();
    parameters.resistance = foc::Ohm{ 0.0f };

    foc::DecouplingFeedforward feedforward;
    feedforward.Configure(ValidParameters());
    feedforward.Configure(parameters);

    const auto output = feedforward.Apply({ 0.1f, 0.2f }, { { 2.0f, 3.0f }, { 0.0f, 0.0f }, 1000.0f });

    EXPECT_NEAR(output.d, 0.1f, tolerance);
    EXPECT_NEAR(output.q, 0.2f, tolerance);
}

TEST_F(TestCurrentPlantModel, a_voltage_vector_inside_the_modulation_circle_is_left_untouched)
{
    const auto output = foc::LimitToModulationCircle({ 0.3f, -0.4f });

    EXPECT_NEAR(output.d, 0.3f, tolerance);
    EXPECT_NEAR(output.q, -0.4f, tolerance);
}

TEST_F(TestCurrentPlantModel, a_voltage_vector_on_the_modulation_circle_is_left_untouched)
{
    const auto output = foc::LimitToModulationCircle({ 0.6f, 0.8f });

    EXPECT_NEAR(output.d, 0.6f, 1e-5f);
    EXPECT_NEAR(output.q, 0.8f, 1e-5f);
}

TEST_F(TestCurrentPlantModel, a_voltage_vector_outside_the_modulation_circle_keeps_its_direction)
{
    const auto output = foc::LimitToModulationCircle({ 3.0f, -4.0f });

    EXPECT_NEAR(std::hypot(output.d, output.q), 1.0f, 1e-5f);
    EXPECT_NEAR(output.d, 0.6f, 1e-5f);
    EXPECT_NEAR(output.q, -0.8f, 1e-5f);
}
