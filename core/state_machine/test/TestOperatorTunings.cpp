#include "core/state_machine/OperatorTunings.hpp"
#include "e-foc-protocol/foc/CommandLimits.hpp"
#include <gtest/gtest.h>
#include <limits>

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
    EXPECT_NEAR(storedCurrent.bandwidth, current.bandwidth, 1e-6f);
    EXPECT_NEAR(storedCurrent.switchingGain, current.switchingGain, 1e-6f);
    EXPECT_NEAR(storedCurrent.boundaryLayer, current.boundaryLayer, 1e-6f);
    EXPECT_TRUE(storedCurrent.twoStepDeadbeat);

    const auto storedSpeed = state_machine::StoredSpeedTunings(config);
    EXPECT_NEAR(storedSpeed.bandwidth, speed.bandwidth, 1e-6f);
    EXPECT_NEAR(storedSpeed.speedErrorWeight, speed.speedErrorWeight, 1e-6f);
    EXPECT_NEAR(storedSpeed.integralWeight, speed.integralWeight, 1e-6f);
    EXPECT_NEAR(storedSpeed.observerBandwidthRatio, speed.observerBandwidthRatio, 1e-6f);
    EXPECT_NEAR(storedSpeed.referenceTimeConstant, speed.referenceTimeConstant, 1e-6f);

    const auto storedPosition = state_machine::StoredPositionTunings(config);
    EXPECT_NEAR(storedPosition.bandwidth, position.bandwidth, 1e-6f);
    EXPECT_NEAR(storedPosition.positionErrorWeight, position.positionErrorWeight, 1e-6f);
    EXPECT_NEAR(storedPosition.speedErrorWeight, position.speedErrorWeight, 1e-6f);
    EXPECT_NEAR(storedPosition.integralWeight, position.integralWeight, 1e-6f);
    EXPECT_NEAR(storedPosition.referenceTimeConstant, position.referenceTimeConstant, 1e-6f);
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
    config.speedLoop.observerBandwidthRatio = 100.0f;
    config.positionLoop.errorWeight = 2.0f;
    config.currentLoop.switchingGain = 0.6f;

    state_machine::SanitizeTunings(config);

    EXPECT_NEAR(config.speedLoop.observerBandwidthRatio, foc::SpeedLoopTunings{}.observerBandwidthRatio, 1e-6f);
    EXPECT_NEAR(config.positionLoop.errorWeight, 2.0f, 1e-6f);
    EXPECT_NEAR(config.currentLoop.switchingGain, foc::CurrentLoopTunings{}.switchingGain, 1e-6f);
}

TEST_F(OperatorTuningsTest, sanitising_replaces_each_supervision_setting_outside_its_range_by_its_default)
{
    constexpr services::SupervisionSettings defaults{};
    config.supervision = { 0.0f, 2.0f, std::numeric_limits<float>::quiet_NaN(), 0.5f, 5.0f };
    config.stallDetection = 7;

    state_machine::SanitizeSupervision(config);

    EXPECT_NEAR(config.supervision.window, defaults.window, 1e-6f);
    EXPECT_NEAR(config.supervision.currentFraction, defaults.currentFraction, 1e-6f);
    EXPECT_NEAR(config.supervision.demandedSpeed, defaults.demandedSpeed, 1e-6f);
    EXPECT_NEAR(config.supervision.positionError, 0.5f, 1e-6f);
    EXPECT_NEAR(config.supervision.stillExcursion, defaults.stillExcursion, 1e-6f);
    EXPECT_EQ(config.stallDetection, uint8_t{ 1 });
}

TEST_F(OperatorTuningsTest, sanitising_replaces_a_limit_outside_its_range_by_its_default)
{
    config.limits.overspeedLimit = 0.5f;
    config.limits.followingErrorWindow = -1.0f;
    config.limits.followingErrorTime = 20.0f;
    config.limits.speedAcceleration = -5.0f;
    config.limits.speedDeceleration = 2.0e5f;
    config.limits.profileVelocity = 6000.0f;
    config.limits.profileAcceleration = -1.0f;
    config.limits.profileDeceleration = 2.0e5f;
    config.limits.profileJerk = 2.0e7f;
    config.limits.commandTimeout = 61.0f;
    config.limits.continuousCurrent = -1.0f;
    config.limits.peakCurrent = 500.0f;
    config.limits.peakCurrentTime = 0.0f;
    config.limits.positionWindow = 3.3f;
    config.limits.positionWindowTime = -0.01f;
    config.limits.positionMinimum = -3.3f;
    config.limits.positionMaximum = 3.3f;
    config.limits.calibrationTemperature = 101.0f;
    config.limits.temperatureLimit = 30.0f;
    config.limits.temperatureMargin = 0.5f;
    config.limits.busVoltageMinimum = -1.0f;
    config.limits.busVoltageMaximum = 101.0f;
    config.limits.quickStopDeceleration = -1.0f;

    state_machine::SanitizeLimits(config);

    EXPECT_NEAR(config.limits.overspeedLimit, services::LimitSettings{}.overspeedLimit, 1e-6f);
    EXPECT_NEAR(config.limits.followingErrorWindow, services::LimitSettings{}.followingErrorWindow, 1e-6f);
    EXPECT_NEAR(config.limits.followingErrorTime, services::LimitSettings{}.followingErrorTime, 1e-6f);
    EXPECT_NEAR(config.limits.speedAcceleration, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.speedDeceleration, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.profileVelocity, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.profileAcceleration, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.profileDeceleration, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.profileJerk, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.commandTimeout, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.continuousCurrent, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.peakCurrent, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.peakCurrentTime, services::LimitSettings{}.peakCurrentTime, 1e-6f);
    EXPECT_NEAR(config.limits.positionWindow, services::LimitSettings{}.positionWindow, 1e-6f);
    EXPECT_NEAR(config.limits.positionWindowTime, services::LimitSettings{}.positionWindowTime, 1e-6f);
    EXPECT_NEAR(config.limits.positionMinimum, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.positionMaximum, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.calibrationTemperature, 25.0f, 1e-6f);
    EXPECT_NEAR(config.limits.temperatureLimit, 120.0f, 1e-6f);
    EXPECT_NEAR(config.limits.temperatureMargin, 15.0f, 1e-6f);
    EXPECT_NEAR(config.limits.busVoltageMinimum, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.busVoltageMaximum, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.quickStopDeceleration, 0.0f, 1e-9f);
}

TEST_F(OperatorTuningsTest, sanitising_keeps_a_limit_within_its_range)
{
    config.limits.overspeedLimit = 50.0f;
    config.limits.followingErrorWindow = 0.3f;
    config.limits.followingErrorTime = 0.05f;
    config.limits.positionWindow = 0.0f;
    config.limits.positionWindowTime = 0.0f;
    config.limits.positionMinimum = -3.0f;
    config.limits.positionMaximum = 3.2f;
    config.limits.busVoltageMinimum = 18.0f;
    config.limits.busVoltageMaximum = 100.0f;
    config.limits.quickStopDeceleration = 500.0f;

    state_machine::SanitizeLimits(config);

    EXPECT_NEAR(config.limits.overspeedLimit, 50.0f, 1e-6f);
    EXPECT_NEAR(config.limits.followingErrorWindow, 0.3f, 1e-6f);
    EXPECT_NEAR(config.limits.followingErrorTime, 0.05f, 1e-6f);
    EXPECT_NEAR(config.limits.positionWindow, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.positionWindowTime, 0.0f, 1e-9f);
    EXPECT_NEAR(config.limits.positionMinimum, -3.0f, 1e-6f);
    EXPECT_NEAR(config.limits.positionMaximum, 3.2f, 1e-6f);
    EXPECT_NEAR(config.limits.busVoltageMinimum, 18.0f, 1e-6f);
    EXPECT_NEAR(config.limits.busVoltageMaximum, 100.0f, 1e-6f);
    EXPECT_NEAR(config.limits.quickStopDeceleration, 500.0f, 1e-6f);
}

TEST_F(OperatorTuningsTest, sanitising_keeps_stall_reporting_turned_off)
{
    config.stallDetection = 0;

    state_machine::SanitizeSupervision(config);

    EXPECT_EQ(config.stallDetection, uint8_t{ 0 });
}

TEST_F(OperatorTuningsTest, sanitising_turns_an_unknown_homing_requirement_off_and_keeps_a_known_one)
{
    config.homingRequired = 2;
    state_machine::SanitizeSupervision(config);
    EXPECT_EQ(uint8_t{ 0 }, config.homingRequired);

    config.homingRequired = 1;
    state_machine::SanitizeSupervision(config);
    EXPECT_EQ(uint8_t{ 1 }, config.homingRequired);
}

TEST_F(OperatorTuningsTest, sanitising_turns_an_unknown_recalibration_mode_off_and_keeps_a_known_one)
{
    config.recalibrationMode = 3;
    state_machine::SanitizeSupervision(config);
    EXPECT_EQ(static_cast<uint8_t>(services::RecalibrationMode::off), config.recalibrationMode);

    config.recalibrationMode = static_cast<uint8_t>(services::RecalibrationMode::automatic);
    state_machine::SanitizeSupervision(config);
    EXPECT_EQ(static_cast<uint8_t>(services::RecalibrationMode::automatic), config.recalibrationMode);
}
