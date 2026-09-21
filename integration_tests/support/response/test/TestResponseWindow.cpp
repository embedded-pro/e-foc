#include "integration_tests/support/response/ResponseWindow.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace integration::response;

    class ResponseWindowTest
        : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            trace.Consume("PLANT_START 100");
            trace.Consume("PLANT 100 1000 10000 100000 0 0");
            trace.Consume("PLANT 120 2000 20000 200000 0 0");
            trace.Consume("PLANT 140 3000 30000 300000 0 0");
            trace.Consume("PLANT 160 4000 40000 400000 0 0");
        }

        PlantTrace trace;
    };
}

TEST_F(ResponseWindowTest, extract_starts_at_the_first_sample_at_or_after_the_tick)
{
    const auto speed = Extract(trace, Signal::speed, 130, 10);

    ASSERT_EQ(speed.size(), 2u);
    EXPECT_NEAR(speed[0], 3.0f, 1e-5f);
    EXPECT_NEAR(speed[1], 4.0f, 1e-5f);
}

TEST_F(ResponseWindowTest, extract_is_capped_at_the_requested_count)
{
    EXPECT_EQ(Extract(trace, Signal::position, 0, 2).size(), 2u);
}

TEST_F(ResponseWindowTest, each_signal_reads_its_own_column)
{
    EXPECT_NEAR(Extract(trace, Signal::position, 100, 1)[0], 0.01f, 1e-6f);
    EXPECT_NEAR(Extract(trace, Signal::currentQ, 100, 1)[0], 0.1f, 1e-6f);
}

TEST_F(ResponseWindowTest, value_at_returns_the_last_sample_not_after_the_tick)
{
    ASSERT_TRUE(ValueAt(trace, Signal::speed, 130).has_value());
    EXPECT_NEAR(*ValueAt(trace, Signal::speed, 130), 2.0f, 1e-5f);
    EXPECT_FALSE(ValueAt(trace, Signal::speed, 50).has_value());
}

TEST_F(ResponseWindowTest, normalise_maps_initial_to_zero_and_reference_to_one)
{
    const auto out = NormaliseStep({ 20.0f, 0.0f, -20.0f }, 20.0f, -20.0f);

    ASSERT_EQ(out.size(), 3u);
    EXPECT_NEAR(out[0], 0.0f, 1e-6f);
    EXPECT_NEAR(out[1], 0.5f, 1e-6f);
    EXPECT_NEAR(out[2], 1.0f, 1e-6f);
}

TEST_F(ResponseWindowTest, signal_words_parse_and_name_round_trip)
{
    ASSERT_TRUE(ParseSignal("speed").has_value());
    EXPECT_EQ(*ParseSignal("position"), Signal::position);
    EXPECT_EQ(*ParseSignal("current"), Signal::currentQ);
    EXPECT_FALSE(ParseSignal("torque").has_value());
    EXPECT_STREQ(SignalUnit(Signal::currentQ), "A");
    EXPECT_EQ(StepWindowSamples(Signal::currentQ), currentWindowSamples);
}
