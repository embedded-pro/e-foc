#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/state_machine/OperatorTunings.hpp"
#include <gtest/gtest.h>

namespace
{
    class OperatorTuningsTest
        : public ::testing::Test
    {
    public:
        services::ConfigData config{};
    };
}

TEST_F(OperatorTuningsTest, every_loop_round_trips_through_the_configuration_record)
{
    const foc::CurrentLoopTunings current{ 5000.0f, 0.3f, 0.9f, true };
    const foc::SpeedLoopTunings speed{ 120.0f, 2.0f, 0.5f, 8.0f, 0.01f };
    const foc::PositionLoopTunings position{ 30.0f, 2.0f, 0.2f, 0.1f, 0.02f };

    state_machine::StoreTunings(config, current);
    state_machine::StoreTunings(config, speed);
    state_machine::StoreTunings(config, position);

    const auto storedCurrent = state_machine::StoredCurrentTunings(config);
    EXPECT_EQ(storedCurrent.bandwidth, current.bandwidth);
    EXPECT_EQ(storedCurrent.switchingGain, current.switchingGain);
    EXPECT_EQ(storedCurrent.boundaryLayer, current.boundaryLayer);
    EXPECT_TRUE(storedCurrent.twoStepDeadbeat);

    const auto storedSpeed = state_machine::StoredSpeedTunings(config);
    EXPECT_EQ(storedSpeed.bandwidth, speed.bandwidth);
    EXPECT_EQ(storedSpeed.speedErrorWeight, speed.speedErrorWeight);
    EXPECT_EQ(storedSpeed.integralWeight, speed.integralWeight);
    EXPECT_EQ(storedSpeed.observerBandwidthRatio, speed.observerBandwidthRatio);
    EXPECT_EQ(storedSpeed.referenceTimeConstant, speed.referenceTimeConstant);

    const auto storedPosition = state_machine::StoredPositionTunings(config);
    EXPECT_EQ(storedPosition.bandwidth, position.bandwidth);
    EXPECT_EQ(storedPosition.positionErrorWeight, position.positionErrorWeight);
    EXPECT_EQ(storedPosition.speedErrorWeight, position.speedErrorWeight);
    EXPECT_EQ(storedPosition.integralWeight, position.integralWeight);
    EXPECT_EQ(storedPosition.referenceTimeConstant, position.referenceTimeConstant);
}

TEST_F(OperatorTuningsTest, a_bandwidth_of_zero_is_automatic_and_acceptable)
{
    EXPECT_TRUE(state_machine::IsAcceptable(state_machine::StoredCurrentTunings(config)));
    EXPECT_TRUE(state_machine::IsAcceptable(state_machine::StoredSpeedTunings(config)));
    EXPECT_TRUE(state_machine::IsAcceptable(state_machine::StoredPositionTunings(config)));
}

TEST_F(OperatorTuningsTest, settings_outside_their_ranges_are_not_acceptable)
{
    auto current = state_machine::StoredCurrentTunings(config);
    current.boundaryLayer = foc::CommandLimits::maxBoundaryLayer * 2.0f;
    EXPECT_FALSE(state_machine::IsAcceptable(current));

    auto speed = state_machine::StoredSpeedTunings(config);
    speed.observerBandwidthRatio = foc::CommandLimits::maxObserverBandwidthRatio + 1.0f;
    EXPECT_FALSE(state_machine::IsAcceptable(speed));

    auto position = state_machine::StoredPositionTunings(config);
    position.bandwidth = foc::CommandLimits::maxPositionBandwidth + 1.0f;
    EXPECT_FALSE(state_machine::IsAcceptable(position));
}

TEST_F(OperatorTuningsTest, a_switching_gain_at_the_boundary_layer_cannot_be_designed)
{
    auto current = state_machine::StoredCurrentTunings(config);
    EXPECT_TRUE(state_machine::IsDesignable(current));

    current.switchingGain = current.boundaryLayer;
    EXPECT_FALSE(state_machine::IsDesignable(current));
}

TEST_F(OperatorTuningsTest, sanitising_resets_only_the_loop_holding_an_unacceptable_value)
{
    config.speedObserverBandwidthRatio = 100.0f;
    config.positionErrorWeight = 2.0f;
    config.currentSwitchingGain = 0.6f;

    state_machine::SanitizeTunings(config);

    EXPECT_EQ(config.speedObserverBandwidthRatio, foc::SpeedLoopTunings{}.observerBandwidthRatio);
    EXPECT_EQ(config.positionErrorWeight, 2.0f);
    EXPECT_EQ(config.currentSwitchingGain, foc::CurrentLoopTunings{}.switchingGain);
}
