#include "core/services/mechanical_system_ident/ObservationQueue.hpp"
#include "gtest/gtest.h"

namespace
{
    using services::IdentificationObservation;
    using services::ObservationQueue;

    IdentificationObservation ObservationAt(uint32_t sample)
    {
        return IdentificationObservation{ .acceleration = 1.0f, .speed = 2.0f, .torque = 3.0f, .sample = sample, .windowSamples = 1 };
    }

    class TestObservationQueue
        : public testing::Test
    {
    public:
        ObservationQueue queue;
    };
}

TEST_F(TestObservationQueue, an_empty_queue_has_nothing_to_take)
{
    EXPECT_FALSE(queue.Take().has_value());
}

TEST_F(TestObservationQueue, observations_are_taken_in_the_order_posted)
{
    queue.Post(ObservationAt(1));
    queue.Post(ObservationAt(2));

    EXPECT_EQ(1u, queue.Take()->sample);
    EXPECT_EQ(2u, queue.Take()->sample);
    EXPECT_FALSE(queue.Take().has_value());
}

TEST_F(TestObservationQueue, reaching_half_capacity_asks_for_a_fit_and_a_full_queue_refuses)
{
    for (uint32_t sample = 1; sample != ObservationQueue::capacity / 2; ++sample)
        EXPECT_EQ(ObservationQueue::PostResult::posted, queue.Post(ObservationAt(sample)));

    EXPECT_EQ(ObservationQueue::PostResult::halfFull, queue.Post(ObservationAt(ObservationQueue::capacity / 2)));

    for (uint32_t sample = ObservationQueue::capacity / 2 + 1; sample <= ObservationQueue::capacity; ++sample)
        EXPECT_EQ(ObservationQueue::PostResult::posted, queue.Post(ObservationAt(sample)));

    EXPECT_EQ(ObservationQueue::PostResult::full, queue.Post(ObservationAt(99)));
}

TEST_F(TestObservationQueue, the_queue_wraps_around_its_storage)
{
    for (uint32_t sample = 0; sample != 3 * ObservationQueue::capacity; ++sample)
    {
        queue.Post(ObservationAt(sample));
        ASSERT_EQ(sample, queue.Take()->sample);
    }
}

TEST_F(TestObservationQueue, clearing_drops_what_was_posted)
{
    queue.Post(ObservationAt(1));
    queue.Clear();

    EXPECT_FALSE(queue.Take().has_value());
}
