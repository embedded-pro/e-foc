#include "core/services/non_volatile_memory/ConfigData.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestConfigData
        : public ::testing::Test
    {
    };
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_can_node_id)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.canNodeId, 1u);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_can_baudrate)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.canBaudrate, 1000000u);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_telemetry_rate)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.telemetryRateHz, 100u);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_encoder_resolution)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.encoderResolution, 4000u);
}

TEST_F(TestConfigData, make_default_config_data_returns_zero_default_control_mode)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.defaultControlMode, uint8_t{ 0 });
}

TEST_F(TestConfigData, config_data_sizeof_is_76)
{
    EXPECT_EQ(sizeof(services::ConfigData), std::size_t{ 76 });
}

TEST_F(TestConfigData, make_default_config_data_leaves_every_bandwidth_automatic)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.currentBandwidth, 0.0f);
    EXPECT_EQ(cfg.speedBandwidth, 0.0f);
    EXPECT_EQ(cfg.positionBandwidth, 0.0f);
}

TEST_F(TestConfigData, make_default_config_data_holds_the_default_law_settings)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.currentSwitchingGain, foc::CurrentLoopTunings{}.switchingGain);
    EXPECT_EQ(cfg.currentBoundaryLayer, foc::CurrentLoopTunings{}.boundaryLayer);
    EXPECT_EQ(cfg.currentTwoStepDeadbeat, uint8_t{ 0 });
    EXPECT_EQ(cfg.speedErrorWeight, foc::SpeedLoopTunings{}.speedErrorWeight);
    EXPECT_EQ(cfg.speedIntegralWeight, foc::SpeedLoopTunings{}.integralWeight);
    EXPECT_EQ(cfg.speedObserverBandwidthRatio, foc::SpeedLoopTunings{}.observerBandwidthRatio);
    EXPECT_EQ(cfg.speedReferenceTimeConstant, foc::SpeedLoopTunings{}.referenceTimeConstant);
    EXPECT_EQ(cfg.positionErrorWeight, foc::PositionLoopTunings{}.positionErrorWeight);
    EXPECT_EQ(cfg.positionSpeedErrorWeight, foc::PositionLoopTunings{}.speedErrorWeight);
    EXPECT_EQ(cfg.positionIntegralWeight, foc::PositionLoopTunings{}.integralWeight);
    EXPECT_EQ(cfg.positionReferenceTimeConstant, foc::PositionLoopTunings{}.referenceTimeConstant);
}

TEST_F(TestConfigData, make_default_config_data_selects_pid_for_both_loops)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.currentAlgorithm, uint8_t{ 0 });
    EXPECT_EQ(cfg.speedAlgorithm, uint8_t{ 0 });
}
