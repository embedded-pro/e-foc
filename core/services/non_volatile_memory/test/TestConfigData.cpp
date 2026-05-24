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

TEST_F(TestConfigData, config_data_sizeof_is_20)
{
    EXPECT_EQ(sizeof(services::ConfigData), std::size_t{ 20 });
}
