#include "core/services/non_volatile_memory/ConfigData.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestConfigData
        : public ::testing::Test
    {
    };
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_max_current)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_FLOAT_EQ(cfg.maxCurrent, 5.0f);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_max_velocity)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_FLOAT_EQ(cfg.maxVelocity, 1000.0f);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_max_torque)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_FLOAT_EQ(cfg.maxTorque, 1.0f);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_can_node_id)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.canNodeId, 1u);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_can_baudrate)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.canBaudrate, 500000u);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_telemetry_rate)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.telemetryRateHz, 100u);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_over_temp_threshold)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_FLOAT_EQ(cfg.overTempThreshold, 80.0f);
}

TEST_F(TestConfigData, make_default_config_data_returns_expected_thresholds)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_FLOAT_EQ(cfg.underVoltageThreshold, 10.0f);
    EXPECT_FLOAT_EQ(cfg.overVoltageThreshold, 28.0f);
}

TEST_F(TestConfigData, make_default_config_data_returns_zero_default_control_mode)
{
    const auto cfg = services::MakeDefaultConfigData();
    EXPECT_EQ(cfg.defaultControlMode, uint8_t{ 0 });
}

TEST_F(TestConfigData, config_data_sizeof_is_40)
{
    EXPECT_EQ(sizeof(services::ConfigData), std::size_t{ 40 });
}
