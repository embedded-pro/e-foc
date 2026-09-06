#include "core/platform_abstraction/CanBusAdapter.hpp"
#include <gtest/gtest.h>

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
    // The error callback mapped busOff to a fault and dropped the other eleven, which is the
    // difference between a bus with marginal termination and one that is unplugged.
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
