#include "can-lite/core/test/CanMock.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using CanError = application::CanBusAdapter::CanError;

    class TestCanErrorCounters
        : public ::testing::Test
    {
    public:
        application::CanBusAdapter::ErrorCounters counters;
    };
}

TEST_F(TestCanErrorCounters, a_fresh_set_of_counters_is_empty)
{
    EXPECT_EQ(0u, counters.Total());
    EXPECT_EQ(0u, counters.Count(CanError::busOff));
}

TEST_F(TestCanErrorCounters, every_class_is_counted_not_just_bus_off)
{
    counters.Record(CanError::busOff);
    counters.Record(CanError::stuffError);
    counters.Record(CanError::stuffError);
    counters.Record(CanError::rxBufferOverflow);

    EXPECT_EQ(1u, counters.Count(CanError::busOff));
    EXPECT_EQ(2u, counters.Count(CanError::stuffError));
    EXPECT_EQ(1u, counters.Count(CanError::rxBufferOverflow));
    EXPECT_EQ(0u, counters.Count(CanError::crcError));
    EXPECT_EQ(4u, counters.Total());
}

TEST_F(TestCanErrorCounters, an_unknown_class_is_counted_as_other_rather_than_indexing_past_the_end)
{
    counters.Record(static_cast<CanError>(99));

    EXPECT_EQ(1u, counters.Count(CanError::other));
    EXPECT_EQ(1u, counters.Total());
}

TEST_F(TestCanErrorCounters, reset_clears_every_class)
{
    counters.Record(CanError::busOff);
    counters.Record(CanError::ackError);
    counters.Reset();

    EXPECT_EQ(0u, counters.Total());
    EXPECT_EQ(0u, counters.Count(CanError::busOff));
    EXPECT_EQ(0u, counters.Count(CanError::ackError));
}

namespace
{
    class TestCanBusAdapterErrorRecording
        : public ::testing::Test
    {
    public:
        application::CanBusAdapterImpl<::testing::StrictMock<hal::CanMock>> adapter;
        std::optional<CanError> reported;

        TestCanBusAdapterErrorRecording()
        {
            adapter.SetOnError([this](CanError error)
                {
                    reported = error;
                });
        }
    };
}

TEST_F(TestCanBusAdapterErrorRecording, an_error_is_counted_on_the_common_base_and_forwarded)
{
    adapter.InvokeErrorHandler(CanError::busOff);

    EXPECT_EQ(1u, adapter.ErrorStatistics().Count(CanError::busOff));
    EXPECT_EQ(1u, adapter.ErrorStatistics().Total());
    ASSERT_TRUE(reported.has_value());
    EXPECT_EQ(CanError::busOff, *reported);
}

TEST_F(TestCanBusAdapterErrorRecording, resetting_the_statistics_clears_what_the_adapter_recorded)
{
    adapter.InvokeErrorHandler(CanError::crcError);
    adapter.ResetErrorStatistics();

    EXPECT_EQ(0u, adapter.ErrorStatistics().Total());
}
