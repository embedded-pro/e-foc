#include "core/services/speed_controllers/SpeedControllerSelector.hpp"
#include <gmock/gmock.h>

namespace
{
    constexpr float tolerance = 1e-4f;

    services::MechanicalModelParameters ValidParameters()
    {
        return { foc::NewtonMeterSecondSquared{ 1.0e-4f }, foc::NewtonMeterSecondPerRadian{ 1.0e-4f },
            foc::NewtonMeter{ 0.05f }, foc::Ampere{ 10.0f }, hal::Hertz{ 1000 } };
    }

    class TestSpeedControllerSelector
        : public ::testing::Test
    {
    public:
        services::SpeedControllerSelector selector;
    };
}

TEST_F(TestSpeedControllerSelector, pid_is_active_by_default)
{
    EXPECT_EQ(selector.Active(), services::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, selection_without_parameters_is_rejected)
{
    EXPECT_EQ(selector.Select(services::SpeedAlgorithm::adrc), services::SelectResult::invalidParameters);
    EXPECT_EQ(selector.Active(), services::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, unknown_algorithm_is_rejected)
{
    selector.Configure(ValidParameters());

    EXPECT_EQ(selector.Select(static_cast<services::SpeedAlgorithm>(42)), services::SelectResult::invalidAlgorithm);
    EXPECT_EQ(selector.Active(), services::SpeedAlgorithm::pid);
}

TEST_F(TestSpeedControllerSelector, every_algorithm_can_be_selected_with_valid_parameters)
{
    using enum services::SpeedAlgorithm;

    selector.Configure(ValidParameters());

    for (auto algorithm : { pid, lqi, adrc, twoDof })
    {
        EXPECT_EQ(selector.Select(algorithm), services::SelectResult::ok);
        EXPECT_EQ(selector.Active(), algorithm);
    }
}

TEST_F(TestSpeedControllerSelector, algorithm_can_be_selected_by_type_at_compile_time)
{
    selector.Configure(ValidParameters());

    EXPECT_EQ(selector.Select<services::AdrcSpeedController>(), services::SelectResult::ok);
    EXPECT_EQ(selector.Active(), services::SpeedAlgorithm::adrc);
}

TEST_F(TestSpeedControllerSelector, dispatch_reaches_the_selected_algorithm)
{
    selector.Configure(ValidParameters());
    selector.Select(services::SpeedAlgorithm::adrc);

    services::AdrcSpeedController expected;
    expected.Configure(ValidParameters());

    const services::SpeedControlContext context{ foc::RadiansPerSecond{ 5.0f }, foc::RadiansPerSecond{ 50.0f } };

    EXPECT_NEAR(selector.Compute(context).Value(), expected.Compute(context).Value(), tolerance);
}

TEST_F(TestSpeedControllerSelector, selection_configures_and_resets_the_new_controller)
{
    selector.Configure(ValidParameters());
    selector.Select(services::SpeedAlgorithm::pid);
    selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 10.0f } });

    selector.Select(services::SpeedAlgorithm::pid);
    auto output = selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 0.0f } });

    EXPECT_NEAR(output.Value(), 0.0f, tolerance);
}

TEST_F(TestSpeedControllerSelector, tunings_are_applied_to_the_active_controller)
{
    selector.Configure(ValidParameters());
    selector.Select(services::SpeedAlgorithm::pid);
    auto reference = selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } });

    selector.SetTunings({ 2.0f * services::SpeedControllerTunings{}.bandwidth, 1.0f, 0.1f, 5.0f, 0.005f });
    selector.Reset();

    EXPECT_GT(selector.Compute({ foc::RadiansPerSecond{ 0.0f }, foc::RadiansPerSecond{ 1.0f } }).Value(), reference.Value());
}
