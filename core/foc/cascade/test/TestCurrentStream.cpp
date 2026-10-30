#include "core/foc/cascade/CurrentStream.hpp"
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>

namespace
{
    class TestCurrentStream
        : public ::testing::Test
    {
    public:
        void AccumulateWindows(uint32_t windows, uint32_t samplesPerWindow)
        {
            for (uint32_t i = 0; i != windows * samplesPerWindow; ++i)
                stream.Accumulate(0.0f, 0.0f, 0.0f);
        }

        foc::CurrentStream stream;
    };
}

TEST_F(TestCurrentStream, a_stopped_stream_publishes_nothing)
{
    for (int i = 0; i != 100; ++i)
        stream.Accumulate(1.0f, 2.0f, 0.5f);

    EXPECT_FALSE(stream.Take().has_value());
}

TEST_F(TestCurrentStream, a_window_publishes_the_mean_currents_of_its_samples)
{
    stream.Start(4);

    stream.Accumulate(1.0f, 2.0f, 0.0f);
    stream.Accumulate(3.0f, 4.0f, 0.0f);
    stream.Accumulate(5.0f, 6.0f, 0.0f);
    stream.Accumulate(7.0f, 8.0f, 0.0f);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 4.0f, 1e-6f);
    EXPECT_NEAR(sample->meanIq.Value(), 5.0f, 1e-6f);
    EXPECT_FALSE(stream.Take().has_value());
}

TEST_F(TestCurrentStream, a_window_reports_the_position_of_its_last_sample)
{
    stream.Start(3);

    stream.Accumulate(0.0f, 0.0f, 0.1f);
    stream.Accumulate(0.0f, 0.0f, 0.2f);
    stream.Accumulate(0.0f, 0.0f, -3.0f);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->position.Value(), -3.0f, 1e-6f);
}

TEST_F(TestCurrentStream, a_partial_window_publishes_nothing)
{
    stream.Start(4);

    stream.Accumulate(1.0f, 1.0f, 0.0f);
    stream.Accumulate(1.0f, 1.0f, 0.0f);
    stream.Accumulate(1.0f, 1.0f, 0.0f);

    EXPECT_FALSE(stream.Take().has_value());
}

TEST_F(TestCurrentStream, consecutive_windows_take_consecutive_sequence_numbers)
{
    stream.Start(2);
    AccumulateWindows(3, 2);

    EXPECT_EQ(stream.Take()->sequence, 0);
    EXPECT_EQ(stream.Take()->sequence, 1);
    EXPECT_EQ(stream.Take()->sequence, 2);
}

TEST_F(TestCurrentStream, the_sequence_wraps_after_256_windows)
{
    stream.Start(1);

    std::optional<foc::CurrentStreamSample> last;
    for (int i = 0; i != 257; ++i)
    {
        stream.Accumulate(0.0f, 0.0f, 0.0f);
        last = stream.Take();
    }

    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->sequence, 0);
}

TEST_F(TestCurrentStream, a_window_closed_while_the_ring_is_full_is_dropped_but_still_counted)
{
    stream.Start(1);
    AccumulateWindows(foc::CurrentStream::capacity + 1, 1);

    for (uint8_t expected = 0; expected != foc::CurrentStream::capacity; ++expected)
        EXPECT_EQ(stream.Take()->sequence, expected);
    EXPECT_FALSE(stream.Take().has_value());

    AccumulateWindows(1, 1);
    EXPECT_EQ(stream.Take()->sequence, foc::CurrentStream::capacity + 1);
}

TEST_F(TestCurrentStream, a_new_window_length_discards_the_partial_window)
{
    stream.Start(4);
    stream.Accumulate(100.0f, 100.0f, 0.0f);
    stream.Accumulate(100.0f, 100.0f, 0.0f);

    stream.Start(2);
    stream.Accumulate(1.0f, 3.0f, 0.0f);
    stream.Accumulate(3.0f, 5.0f, 0.0f);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 2.0f, 1e-6f);
    EXPECT_NEAR(sample->meanIq.Value(), 4.0f, 1e-6f);
}

TEST_F(TestCurrentStream, a_restart_discards_the_partial_window_and_keeps_the_length)
{
    stream.Start(3);
    stream.Accumulate(100.0f, 100.0f, 0.0f);
    stream.Accumulate(100.0f, 100.0f, 0.0f);

    stream.Restart();
    stream.Accumulate(1.0f, 2.0f, 0.0f);
    stream.Accumulate(1.0f, 2.0f, 0.0f);
    EXPECT_FALSE(stream.Take().has_value());
    stream.Accumulate(1.0f, 2.0f, 0.0f);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 1.0f, 1e-6f);
    EXPECT_NEAR(sample->meanIq.Value(), 2.0f, 1e-6f);
}

TEST_F(TestCurrentStream, a_restart_keeps_the_windows_already_published)
{
    stream.Start(1);
    AccumulateWindows(2, 1);

    stream.Restart();

    EXPECT_EQ(stream.Take()->sequence, 0);
    EXPECT_EQ(stream.Take()->sequence, 1);
}

TEST_F(TestCurrentStream, a_stop_discards_the_partial_window)
{
    stream.Start(2);
    stream.Accumulate(100.0f, 100.0f, 0.0f);

    stream.Stop();
    stream.Accumulate(100.0f, 100.0f, 0.0f);
    EXPECT_FALSE(stream.Take().has_value());

    stream.Start(2);
    stream.Accumulate(1.0f, 1.0f, 0.0f);
    stream.Accumulate(1.0f, 1.0f, 0.0f);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 1.0f, 1e-6f);
}

TEST_F(TestCurrentStream, a_start_or_a_stop_drops_the_windows_not_yet_taken)
{
    stream.Start(1);
    AccumulateWindows(3, 1);
    stream.Stop();
    EXPECT_FALSE(stream.Take().has_value());

    stream.Start(1);
    AccumulateWindows(2, 1);
    stream.Start(1);
    EXPECT_FALSE(stream.Take().has_value());
}
