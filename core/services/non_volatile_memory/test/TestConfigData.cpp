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

TEST_F(TestConfigData, config_data_sizeof_is_96)
{
    EXPECT_EQ(sizeof(services::ConfigData), std::size_t{ 96 });
}

TEST_F(TestConfigData, make_default_config_data_holds_the_default_supervision_settings)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_NEAR(cfg.supervision.window, 0.1f, 1e-6f);
    EXPECT_NEAR(cfg.supervision.currentFraction, 0.05f, 1e-6f);
    EXPECT_NEAR(cfg.supervision.demandedSpeed, 2.0f, 1e-6f);
    EXPECT_NEAR(cfg.supervision.positionError, 0.2f, 1e-6f);
    EXPECT_NEAR(cfg.supervision.stillExcursion, 0.01f, 1e-6f);
    EXPECT_EQ(cfg.stallDetection, uint8_t{ 1 });
}

TEST_F(TestConfigData, make_default_config_data_leaves_every_bandwidth_automatic)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_NEAR(cfg.currentLoop.bandwidth, 0.0f, 1e-9f);
    EXPECT_NEAR(cfg.speedLoop.bandwidth, 0.0f, 1e-9f);
    EXPECT_NEAR(cfg.positionLoop.bandwidth, 0.0f, 1e-9f);
}

TEST_F(TestConfigData, make_default_config_data_holds_the_default_law_settings)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_NEAR(cfg.currentLoop.switchingGain, foc::CurrentLoopTunings{}.switchingGain, 1e-6f);
    EXPECT_NEAR(cfg.currentLoop.boundaryLayer, foc::CurrentLoopTunings{}.boundaryLayer, 1e-6f);
    EXPECT_EQ(cfg.currentTwoStepDeadbeat, uint8_t{ 0 });
    EXPECT_NEAR(cfg.speedLoop.errorWeight, foc::SpeedLoopTunings{}.speedErrorWeight, 1e-6f);
    EXPECT_NEAR(cfg.speedLoop.integralWeight, foc::SpeedLoopTunings{}.integralWeight, 1e-6f);
    EXPECT_NEAR(cfg.speedLoop.observerBandwidthRatio, foc::SpeedLoopTunings{}.observerBandwidthRatio, 1e-6f);
    EXPECT_NEAR(cfg.speedLoop.referenceTimeConstant, foc::SpeedLoopTunings{}.referenceTimeConstant, 1e-6f);
    EXPECT_NEAR(cfg.positionLoop.errorWeight, foc::PositionLoopTunings{}.positionErrorWeight, 1e-6f);
    EXPECT_NEAR(cfg.positionLoop.speedErrorWeight, foc::PositionLoopTunings{}.speedErrorWeight, 1e-6f);
    EXPECT_NEAR(cfg.positionLoop.integralWeight, foc::PositionLoopTunings{}.integralWeight, 1e-6f);
    EXPECT_NEAR(cfg.positionLoop.referenceTimeConstant, foc::PositionLoopTunings{}.referenceTimeConstant, 1e-6f);
}

TEST_F(TestConfigData, make_default_config_data_selects_pid_for_both_loops)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.currentAlgorithm, uint8_t{ 0 });
    EXPECT_EQ(cfg.speedAlgorithm, uint8_t{ 0 });
}
