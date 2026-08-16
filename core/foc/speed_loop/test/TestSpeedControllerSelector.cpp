#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    foc::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    class TestSpeedControllerSelector
        : public ::testing::Test
    {
    public:
        foc::SpeedControllerSelector selector;
    };
}

TEST_F(TestSpeedControllerSelector, pid_is_active_by_default)
{
    EXPECT_EQ(selector.Active(), foc::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, model_based_selection_without_parameters_is_rejected)
{
    EXPECT_EQ(selector.Select(foc::SpeedAlgorithm::adrc), foc::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Select(foc::SpeedAlgorithm::lqi), foc::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Active(), foc::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, model_free_selection_without_parameters_is_accepted)
{
    EXPECT_EQ(selector.Select(foc::SpeedAlgorithm::twoDof), foc::SelectResult::ok);
    EXPECT_EQ(selector.Active(), foc::SpeedAlgorithm::twoDof);

    EXPECT_EQ(selector.Select(foc::SpeedAlgorithm::pid), foc::SelectResult::ok);
    EXPECT_EQ(selector.Active(), foc::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, unknown_algorithm_is_rejected)
{
    selector.Configure(ValidParameters());

    EXPECT_EQ(selector.Select(static_cast<foc::SpeedAlgorithm>(42)), foc::SelectResult::invalidAlgorithm);
    EXPECT_EQ(selector.Active(), foc::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, every_algorithm_can_be_selected_with_valid_parameters)
{
    using enum foc::SpeedAlgorithm;

    selector.Configure(ValidParameters());

    for (auto algorithm : { pid, lqi, adrc, twoDof })
    {
        EXPECT_EQ(selector.Select(algorithm), foc::SelectResult::ok);
        EXPECT_EQ(selector.Active(), algorithm);
    }
}

TEST_F(TestSpeedControllerSelector, algorithm_can_be_selected_by_type_at_compile_time)
{
    selector.Configure(ValidParameters());

    EXPECT_EQ(selector.Select<foc::AdrcSpeedController>(), foc::SelectResult::ok);
    EXPECT_EQ(selector.Active(), foc::SpeedAlgorithm::adrc);
}

TEST_F(TestSpeedControllerSelector, dispatch_reaches_the_selected_algorithm)
{
    selector.Configure(ValidParameters());
    selector.Select(foc::SpeedAlgorithm::adrc);

    foc::AdrcSpeedController expected;
    expected.Configure(ValidParameters());

    const foc::SpeedControlContext context{ foc::RadiansPerSecond{ 5.0f }, foc::RadiansPerSecond{ 50.0f } };

    EXPECT_NEAR(selector.Compute(context).Value(), expected.Compute(context).Value(), tolerance);
}

TEST_F(TestSpeedControllerSelector, selection_configures_and_resets_the_new_controller)
{
    selector.Configure(ValidParameters());
    selector.Select(foc::SpeedAlgorithm::pid);
    selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    selector.Select(foc::SpeedAlgorithm::pid);
    auto output = selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestSpeedControllerSelector, tunings_are_applied_to_the_active_controller)
{
    selector.Configure(ValidParameters());
    selector.Select(foc::SpeedAlgorithm::pid);
    auto reference = selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } });

    selector.SetTunings({ 2.0f * foc::SpeedLoopTunings{}.bandwidth, 1.0f, 0.1f, 5.0f, 0.005f });
    selector.Reset();

    EXPECT_GT(selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } }).Value(), reference.Value());
}
