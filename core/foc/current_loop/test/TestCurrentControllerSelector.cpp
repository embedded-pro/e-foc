#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    foc::MotorModelParameters ValidParameters()
    {
        return { foc::Ohm{ 1.0f }, foc::MilliHenry{ 0.5f }, foc::Weber{ 0.01f }, foc::Volts{ 24.0f }, hal::Hertz{ 20000 } };
    }

    foc::MotorModelParameters ParametersWithoutFluxLinkage()
    {
        auto parameters = ValidParameters();
        parameters.fluxLinkage = foc::Weber{ 0.0f };

        return parameters;
    }

    class TestCurrentControllerSelector
        : public ::testing::Test
    {
    public:
        foc::CurrentControllerSelector selector;
    };
}

TEST_F(TestCurrentControllerSelector, pid_is_active_by_default)
{
    EXPECT_EQ(selector.Active(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestCurrentControllerSelector, selection_without_parameters_is_rejected)
{
    EXPECT_EQ(selector.Select(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Active(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestCurrentControllerSelector, unknown_algorithm_is_rejected)
{
    selector.Configure(ValidParameters());

    EXPECT_EQ(selector.Select(static_cast<foc::CurrentAlgorithm>(42)), foc::SelectResult::invalidAlgorithm);
    EXPECT_EQ(selector.Active(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestCurrentControllerSelector, decoupled_pid_requires_flux_linkage)
{
    selector.Configure(ParametersWithoutFluxLinkage());

    EXPECT_EQ(selector.Select(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Select(foc::CurrentAlgorithm::slidingMode), foc::SelectResult::ok);
}

TEST_F(TestCurrentControllerSelector, every_algorithm_can_be_selected_with_valid_parameters)
{
    using enum foc::CurrentAlgorithm;

    selector.Configure(ValidParameters());

    for (auto algorithm : { pid, decoupledPid, deadbeat, slidingMode })
    {
        EXPECT_EQ(selector.Select(algorithm), foc::SelectResult::ok);
        EXPECT_EQ(selector.Active(), algorithm);
    }
}

TEST_F(TestCurrentControllerSelector, algorithm_can_be_selected_by_type_at_compile_time)
{
    selector.Configure(ValidParameters());

    EXPECT_EQ(selector.Select<foc::DeadbeatCurrentController>(), foc::SelectResult::ok);
    EXPECT_EQ(selector.Active(), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(TestCurrentControllerSelector, selection_by_type_reports_the_same_identity_as_selection_by_enum)
{
    selector.Configure(ValidParameters());

    selector.Select<foc::SlidingModeCurrentController>();
    auto byType = selector.Active();

    selector.Select(foc::CurrentAlgorithm::slidingMode);

    EXPECT_EQ(byType, selector.Active());
}

TEST_F(TestCurrentControllerSelector, dispatch_reaches_the_selected_algorithm)
{
    selector.Configure(ValidParameters());
    selector.Select(foc::CurrentAlgorithm::deadbeat);

    foc::DeadbeatCurrentController expected;
    expected.Configure(ValidParameters());

    const foc::CurrentControlContext context{ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f };
    auto output = selector.Compute(context);

    EXPECT_NEAR(output.d, expected.Compute(context).d, tolerance);
    EXPECT_NEAR(output.q, expected.Compute(context).q, tolerance);
}

TEST_F(TestCurrentControllerSelector, selection_configures_and_resets_the_new_controller)
{
    selector.Configure(ValidParameters());
    selector.Select(foc::CurrentAlgorithm::pid);
    selector.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    selector.Select(foc::CurrentAlgorithm::pid);
    auto output = selector.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}

TEST_F(TestCurrentControllerSelector, tunings_are_applied_to_the_active_controller)
{
    selector.Configure(ValidParameters());
    selector.Select(foc::CurrentAlgorithm::deadbeat);

    const foc::CurrentControlContext context{ { 0.05f, 0.05f }, { 0.1f, 0.1f }, 0.0f };
    auto oneStep = selector.Compute(context);

    selector.SetTunings({ 6283.185307f, 1.0f, 0.2f, true });
    auto twoStep = selector.Compute(context);

    EXPECT_LT(twoStep.d, oneStep.d);
}

TEST_F(TestCurrentControllerSelector, reset_clears_the_active_controller_state)
{
    selector.Configure(ValidParameters());
    selector.Compute({ { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f });

    selector.Reset();
    auto output = selector.Compute({ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f });

    EXPECT_NEAR(output.d, 0.0f, tolerance);
    EXPECT_NEAR(output.q, 0.0f, tolerance);
}
