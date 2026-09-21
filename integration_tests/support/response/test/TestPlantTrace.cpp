#include "integration_tests/support/response/PlantTrace.hpp"
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    class PlantTraceTest
        : public ::testing::Test
    {
    protected:
        integration::response::PlantTrace trace;
    };
}

TEST_F(PlantTraceTest, ignores_lines_that_are_not_plant_lines)
{
    EXPECT_FALSE(trace.Consume("CAN_TX 1234 0102"));
    EXPECT_FALSE(trace.Consume("[SM] Speed loop algorithm: pid"));
    EXPECT_FALSE(trace.Consume(""));
    EXPECT_FALSE(trace.Consume("PLANTX 1 2 3 4 5 6"));
    EXPECT_TRUE(trace.Samples().empty());
}

TEST_F(PlantTraceTest, parses_a_sample_line_in_milli_and_micro_units)
{
    ASSERT_TRUE(trace.Consume("PLANT 40 20000 1500000 500000 -2000 3000"));

    ASSERT_EQ(trace.Samples().size(), 1u);
    const auto& sample = trace.Samples().front();
    EXPECT_EQ(sample.tick, 40u);
    EXPECT_NEAR(sample.omegaMech, 20.0f, 1e-4f);
    EXPECT_NEAR(sample.thetaMech, 1.5f, 1e-5f);
    EXPECT_NEAR(sample.iq, 0.5f, 1e-6f);
    EXPECT_NEAR(sample.id, -0.002f, 1e-6f);
    EXPECT_NEAR(sample.externalTorqueNm, 0.003f, 1e-6f);
}

TEST_F(PlantTraceTest, rejects_a_truncated_sample_line)
{
    EXPECT_FALSE(trace.Consume("PLANT 40 20000 1500000"));
    EXPECT_TRUE(trace.Samples().empty());
}

TEST_F(PlantTraceTest, start_marker_clears_earlier_samples_and_records_the_tick)
{
    trace.Consume("PLANT 5 0 0 0 0 0");
    ASSERT_TRUE(trace.Consume("PLANT_START 100"));
    trace.Consume("PLANT 100 0 0 0 0 0");

    ASSERT_TRUE(trace.StartTick().has_value());
    EXPECT_EQ(*trace.StartTick(), 100u);
    ASSERT_EQ(trace.Samples().size(), 1u);
    EXPECT_EQ(trace.Samples().front().tick, 100u);
}

TEST_F(PlantTraceTest, event_and_stop_markers_are_recorded)
{
    ASSERT_TRUE(trace.Consume("PLANT_EVENT 300 torque 2000"));
    ASSERT_TRUE(trace.Consume("PLANT_STOP 900 3"));

    ASSERT_TRUE(trace.EventTick("torque").has_value());
    EXPECT_EQ(*trace.EventTick("torque"), 300u);
    EXPECT_NEAR(trace.Events().front().value, 0.002f, 1e-7f);
    ASSERT_TRUE(trace.StopTick().has_value());
    EXPECT_EQ(*trace.StopTick(), 900u);
    EXPECT_EQ(trace.Dropped(), 3u);
    EXPECT_FALSE(trace.EventTick("other").has_value());
}

TEST_F(PlantTraceTest, command_stamp_carries_tick_id_and_payload)
{
    ASSERT_TRUE(trace.Consume("CAN_RX_AT 250 12345678 010014"));

    ASSERT_TRUE(trace.CommandTick(0x12345678u).has_value());
    EXPECT_EQ(*trace.CommandTick(0x12345678u), 250u);
    EXPECT_FALSE(trace.CommandTick(0x1u).has_value());
    ASSERT_EQ(trace.Commands().front().payload.size(), 3u);
    EXPECT_EQ(trace.Commands().front().payload[2], 0x14u);
}

TEST_F(PlantTraceTest, last_command_stamp_wins)
{
    trace.Consume("CAN_RX_AT 10 abc 00");
    trace.Consume("CAN_RX_AT 20 abc 00");

    EXPECT_EQ(*trace.CommandTick(0xabcu), 20u);
}

TEST_F(PlantTraceTest, spacing_and_gaps_reflect_uniform_sampling)
{
    trace.Consume("PLANT 0 0 0 0 0 0");
    trace.Consume("PLANT 20 0 0 0 0 0");
    trace.Consume("PLANT 40 0 0 0 0 0");
    trace.Consume("PLANT 80 0 0 0 0 0");

    ASSERT_TRUE(trace.TickSpacing().has_value());
    EXPECT_EQ(*trace.TickSpacing(), 20u);
    EXPECT_EQ(trace.Gaps(), 1u);
    EXPECT_EQ(*trace.LastSampleTick(), 80u);
}

TEST_F(PlantTraceTest, position_is_unwrapped_across_the_pi_boundary)
{
    trace.Consume("PLANT 0 0 3000000 0 0 0");
    trace.Consume("PLANT 1 0 -3000000 0 0 0");
    trace.Consume("PLANT 2 0 -2900000 0 0 0");

    const auto& samples = trace.Samples();
    EXPECT_NEAR(samples[1].thetaMech, -3.0f + 2.0f * std::numbers::pi_v<float>, 1e-4f);
    EXPECT_NEAR(samples[2].thetaMech, -2.9f + 2.0f * std::numbers::pi_v<float>, 1e-4f);
}

TEST_F(PlantTraceTest, clear_forgets_everything)
{
    trace.Consume("PLANT_START 1");
    trace.Consume("PLANT 1 0 0 0 0 0");
    trace.Consume("CAN_RX_AT 2 abc 00");
    trace.Clear();

    EXPECT_TRUE(trace.Samples().empty());
    EXPECT_TRUE(trace.Commands().empty());
    EXPECT_FALSE(trace.StartTick().has_value());
}
