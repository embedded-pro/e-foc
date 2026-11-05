#include "core/services/bus_voltage/BusVoltageMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace
{
    class BusVoltageMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        float bus{ 24.0f };
        std::vector<float> tracked;
        services::BusVoltageMonitor monitor{ [this]()
            {
                return foc::Volts{ bus };
            },
            [this](foc::Volts measured)
            {
                tracked.push_back(measured.Value());
            } };
    };
}

TEST_F(BusVoltageMonitorTest, nothing_is_measured_before_the_monitor_starts)
{
    ForwardTime(std::chrono::milliseconds(100));

    EXPECT_TRUE(tracked.empty());
}

TEST_F(BusVoltageMonitorTest, the_bus_is_measured_every_10_ms)
{
    monitor.Start();

    ForwardTime(std::chrono::milliseconds(9));
    EXPECT_TRUE(tracked.empty());

    ForwardTime(std::chrono::milliseconds(41));
    EXPECT_EQ(5u, tracked.size());
}

TEST_F(BusVoltageMonitorTest, the_first_measurement_is_handed_on_as_it_is)
{
    monitor.Start();

    ForwardTime(std::chrono::milliseconds(10));

    ASSERT_EQ(1u, tracked.size());
    EXPECT_FLOAT_EQ(24.0f, tracked.front());
}

TEST_F(BusVoltageMonitorTest, a_step_of_the_bus_is_followed_with_a_50_ms_time_constant)
{
    monitor.Start();
    ForwardTime(std::chrono::milliseconds(10));

    bus = 12.0f;
    ForwardTime(std::chrono::milliseconds(50));

    const float alpha = 0.01f / (0.05f + 0.01f);
    float expected = 24.0f;
    for (int poll = 0; poll != 5; ++poll)
        expected += alpha * (12.0f - expected);
    EXPECT_NEAR(expected, tracked.back(), 1e-4f);
    EXPECT_LT(tracked.back(), 18.0f);
}

TEST_F(BusVoltageMonitorTest, a_stopped_monitor_measures_no_more)
{
    monitor.Start();
    ForwardTime(std::chrono::milliseconds(20));

    monitor.Stop();
    ForwardTime(std::chrono::milliseconds(100));

    EXPECT_EQ(2u, tracked.size());
}
