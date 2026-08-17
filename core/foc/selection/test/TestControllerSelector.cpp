#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    foc::MotorModelParameters ValidElectricalParameters()
    {
        return { foc::Ohm{ 1.0f }, foc::MilliHenry{ 0.5f }, foc::Weber{ 0.01f }, foc::Volts{ 24.0f }, hal::Hertz{ 20000 } };
    }

    foc::MotorModelParameters ElectricalParametersWithoutFluxLinkage()
    {
        auto parameters = ValidElectricalParameters();
        parameters.fluxLinkage = foc::Weber{ 0.0f };

        return parameters;
    }

    foc::MechanicalModelParameters ValidMechanicalParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    foc::MechanicalModelParameters MechanicalParametersWithoutInertia()
    {
        auto parameters = ValidMechanicalParameters();
        parameters.inertia = foc::NewtonMeterSecondSquared{ 0.0f };

        return parameters;
    }

    foc::CurrentLoopTunings CurrentTuningsWithBandwidth(float bandwidth)
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = bandwidth;

        return tunings;
    }

    foc::SpeedLoopTunings SpeedTuningsWithWeights(float speedErrorWeight, float integralWeight)
    {
        auto tunings = foc::SpeedLoopTunings{};
        tunings.speedErrorWeight = speedErrorWeight;
        tunings.integralWeight = integralWeight;

        return tunings;
    }

    const foc::CurrentControlContext currentStep{ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f };
    const foc::SpeedControlContext speedStep{ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } };

    class TestControllerSelector
        : public ::testing::Test
    {
    public:
        foc::CurrentControllerSelector currentSelector;
        foc::SpeedControllerSelector speedSelector;
    };
}

TEST_F(TestControllerSelector, the_first_variant_alternative_is_active_before_any_selection)
{
    EXPECT_EQ(currentSelector.Active(), foc::PidCurrentController::algorithm);
    EXPECT_EQ(speedSelector.Active(), foc::PidSpeedController::algorithm);
}

TEST_F(TestControllerSelector, the_default_alternative_already_dispatches_without_a_selection)
{
    currentSelector.Configure(ValidElectricalParameters());

    foc::PidCurrentController expected;
    expected.Configure(ValidElectricalParameters());

    const auto output = currentSelector.Compute(currentStep);
    const auto reference = expected.Compute(currentStep);

    EXPECT_NEAR(output.d, reference.d, tolerance);
    EXPECT_NEAR(output.q, reference.q, tolerance);
}

TEST_F(TestControllerSelector, selection_by_type_and_by_identifier_reach_the_same_alternative)
{
    foc::CurrentControllerSelector byIdentifier;

    currentSelector.Configure(ValidElectricalParameters());
    byIdentifier.Configure(ValidElectricalParameters());

    ASSERT_EQ(currentSelector.Select<foc::SlidingModeCurrentController>(), foc::SelectResult::ok);
    ASSERT_EQ(byIdentifier.Select(foc::CurrentAlgorithm::slidingMode), foc::SelectResult::ok);

    EXPECT_EQ(currentSelector.Active(), byIdentifier.Active());

    const auto byType = currentSelector.Compute(currentStep);
    const auto byEnum = byIdentifier.Compute(currentStep);

    EXPECT_NEAR(byType.d, byEnum.d, tolerance);
    EXPECT_NEAR(byType.q, byEnum.q, tolerance);
}

TEST_F(TestControllerSelector, selection_carries_the_configuration_and_the_tunings_into_the_new_alternative)
{
    currentSelector.Configure(ValidElectricalParameters());
    currentSelector.SetTunings(CurrentTuningsWithBandwidth(3000.0f));
    ASSERT_EQ(currentSelector.Select(foc::CurrentAlgorithm::pid), foc::SelectResult::ok);

    foc::PidCurrentController expected;
    expected.Configure(ValidElectricalParameters());
    expected.SetTunings(CurrentTuningsWithBandwidth(3000.0f));

    const auto output = currentSelector.Compute(currentStep);
    const auto reference = expected.Compute(currentStep);

    EXPECT_NEAR(output.d, reference.d, tolerance);
    EXPECT_NEAR(output.q, reference.q, tolerance);
}

TEST_F(TestControllerSelector, compute_dispatches_to_the_selected_alternative)
{
    speedSelector.Configure(ValidMechanicalParameters());
    ASSERT_EQ(speedSelector.Select(foc::SpeedAlgorithm::adrc), foc::SelectResult::ok);

    foc::AdrcSpeedController expected;
    expected.Configure(ValidMechanicalParameters());

    for (int sample = 0; sample != 5; ++sample)
        EXPECT_NEAR(speedSelector.Compute(speedStep).Value(), expected.Compute(speedStep).Value(), tolerance);
}

TEST_F(TestControllerSelector, an_unknown_identifier_leaves_the_running_alternative_untouched)
{
    currentSelector.Configure(ValidElectricalParameters());
    ASSERT_EQ(currentSelector.Select(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);

    EXPECT_EQ(currentSelector.Select(static_cast<foc::CurrentAlgorithm>(42)), foc::SelectResult::invalidAlgorithm);
    EXPECT_EQ(currentSelector.Active(), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(TestControllerSelector, a_rejected_selection_leaves_the_previous_alternative_active_and_dispatching)
{
    currentSelector.Configure(ValidElectricalParameters());
    ASSERT_EQ(currentSelector.Select(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    currentSelector.Configure(ElectricalParametersWithoutFluxLinkage());

    EXPECT_EQ(currentSelector.Select(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::invalidParameters);
    EXPECT_EQ(currentSelector.Active(), foc::CurrentAlgorithm::deadbeat);

    foc::DeadbeatCurrentController stillRunning;
    stillRunning.Configure(ElectricalParametersWithoutFluxLinkage());

    const auto output = currentSelector.Compute(currentStep);
    const auto reference = stillRunning.Compute(currentStep);

    EXPECT_NEAR(output.d, reference.d, tolerance);
    EXPECT_NEAR(output.q, reference.q, tolerance);
}

TEST_F(TestControllerSelector, the_active_algorithm_reports_the_running_alternative_rather_than_the_last_request)
{
    speedSelector.Configure(ValidMechanicalParameters());
    ASSERT_EQ(speedSelector.Select(foc::SpeedAlgorithm::adrc), foc::SelectResult::ok);
    speedSelector.Configure(MechanicalParametersWithoutInertia());

    EXPECT_EQ(speedSelector.Select(foc::SpeedAlgorithm::lqi), foc::SelectResult::invalidParameters);
    EXPECT_EQ(speedSelector.Active(), foc::SpeedAlgorithm::adrc);
}

TEST_F(TestControllerSelector, accepted_tunings_reach_the_active_current_controller)
{
    currentSelector.Configure(ValidElectricalParameters());

    EXPECT_EQ(currentSelector.TrySetTunings(CurrentTuningsWithBandwidth(3000.0f)), foc::SelectResult::ok);

    foc::PidCurrentController expected;
    expected.Configure(ValidElectricalParameters());
    expected.SetTunings(CurrentTuningsWithBandwidth(3000.0f));

    const auto output = currentSelector.Compute(currentStep);
    const auto reference = expected.Compute(currentStep);

    EXPECT_NEAR(output.d, reference.d, tolerance);
    EXPECT_NEAR(output.q, reference.q, tolerance);
}

TEST_F(TestControllerSelector, tunings_offered_without_an_electrical_model_are_refused_and_not_stored)
{
    EXPECT_EQ(currentSelector.TrySetTunings(CurrentTuningsWithBandwidth(3000.0f)), foc::SelectResult::invalidParameters);

    currentSelector.Configure(ValidElectricalParameters());

    foc::PidCurrentController onDefaultTunings;
    onDefaultTunings.Configure(ValidElectricalParameters());

    const auto output = currentSelector.Compute(currentStep);
    const auto reference = onDefaultTunings.Compute(currentStep);

    EXPECT_NEAR(output.d, reference.d, tolerance);
    EXPECT_NEAR(output.q, reference.q, tolerance);
}

TEST_F(TestControllerSelector, accepted_tunings_reach_the_active_speed_controller)
{
    speedSelector.Configure(ValidMechanicalParameters());
    ASSERT_EQ(speedSelector.Select(foc::SpeedAlgorithm::lqi), foc::SelectResult::ok);

    EXPECT_EQ(speedSelector.TrySetTunings(SpeedTuningsWithWeights(4.0f, 0.5f)), foc::SelectResult::ok);

    foc::LqiSpeedController expected;
    expected.Configure(ValidMechanicalParameters());
    expected.SetTunings(SpeedTuningsWithWeights(4.0f, 0.5f));

    EXPECT_NEAR(speedSelector.Compute(speedStep).Value(), expected.Compute(speedStep).Value(), tolerance);
}

TEST_F(TestControllerSelector, tunings_the_active_speed_algorithm_cannot_be_designed_for_leave_the_last_accepted_set_live)
{
    speedSelector.Configure(ValidMechanicalParameters());
    ASSERT_EQ(speedSelector.Select(foc::SpeedAlgorithm::lqi), foc::SelectResult::ok);
    ASSERT_EQ(speedSelector.TrySetTunings(SpeedTuningsWithWeights(4.0f, 0.5f)), foc::SelectResult::ok);

    speedSelector.Configure(MechanicalParametersWithoutInertia());
    EXPECT_EQ(speedSelector.TrySetTunings(SpeedTuningsWithWeights(0.01f, 0.001f)), foc::SelectResult::invalidParameters);

    speedSelector.Configure(ValidMechanicalParameters());

    foc::LqiSpeedController onAcceptedTunings;
    onAcceptedTunings.Configure(ValidMechanicalParameters());
    onAcceptedTunings.SetTunings(SpeedTuningsWithWeights(4.0f, 0.5f));

    EXPECT_NEAR(speedSelector.Compute(speedStep).Value(), onAcceptedTunings.Compute(speedStep).Value(), tolerance);
}
