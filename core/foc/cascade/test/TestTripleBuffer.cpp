#include "core/foc/cascade/TripleBuffer.hpp"
#include <cstddef>
#include <gtest/gtest.h>

namespace
{
    struct Pair
    {
        float first{ 0.0f };
        float second{ 0.0f };
    };

    class TestTripleBuffer
        : public ::testing::Test
    {
    public:
        foc::TripleBuffer<Pair> buffer;
    };
}

TEST_F(TestTripleBuffer, acquire_before_any_publish_returns_the_default_value)
{
    const auto& value = buffer.Acquire();

    EXPECT_EQ(value.first, 0.0f);
    EXPECT_EQ(value.second, 0.0f);
}

TEST_F(TestTripleBuffer, acquire_returns_the_most_recently_published_value)
{
    buffer.Publish({ 1.0f, 2.0f });

    const auto& value = buffer.Acquire();

    EXPECT_EQ(value.first, 1.0f);
    EXPECT_EQ(value.second, 2.0f);
}

TEST_F(TestTripleBuffer, a_new_publish_never_tears_a_value_already_acquired)
{
    buffer.Publish({ 1.0f, 1.0f });
    const auto& held = buffer.Acquire();

    buffer.Publish({ 2.0f, 2.0f });

    EXPECT_EQ(held.first, 1.0f);
    EXPECT_EQ(held.second, 1.0f);
}

TEST_F(TestTripleBuffer, fields_published_together_are_always_acquired_together)
{
    for (int i = 0; i != 10; ++i)
    {
        const auto value = static_cast<float>(i);
        buffer.Publish({ value, -value });

        const auto& acquired = buffer.Acquire();

        EXPECT_EQ(acquired.first, value);
        EXPECT_EQ(acquired.second, -value);
    }
}

TEST_F(TestTripleBuffer, a_stalled_reader_still_reads_a_consistent_pair_after_many_publishes)
{
    buffer.Publish({ 42.0f, -42.0f });
    const auto& held = buffer.Acquire();

    for (int i = 0; i != 100; ++i)
        buffer.Publish({ static_cast<float>(i), static_cast<float>(-i) });

    EXPECT_EQ(held.first, 42.0f);
    EXPECT_EQ(held.second, -42.0f);
}

namespace
{
    constexpr uint8_t slotCount = 3;

    class TripleBufferSlotTest
        : public ::testing::Test
    {
    public:
        static uint8_t Advance(uint8_t& ready, uint8_t writeSlot, uint8_t held)
        {
            ready = writeSlot;
            return foc::detail::NextFreeSlot(slotCount, ready, held);
        }
    };
}

TEST_F(TripleBufferSlotTest, the_writer_never_lands_on_the_slot_it_just_published)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
        for (uint8_t held = 0; held != slotCount; ++held)
            EXPECT_NE(ready, foc::detail::NextFreeSlot(slotCount, ready, held)) << " ready=" << +ready << " held=" << +held;
}

TEST_F(TripleBufferSlotTest, the_writer_never_lands_on_the_slot_the_reader_holds)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
    {
        for (uint8_t held = 0; held != slotCount; ++held)
        {
            if (ready != held)
            {
                EXPECT_NE(held, foc::detail::NextFreeSlot(slotCount, ready, held)) << " ready=" << +ready << " held=" << +held;
            }
        }
    }
}

TEST_F(TripleBufferSlotTest, the_chosen_slot_is_always_in_range)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
        for (uint8_t held = 0; held != slotCount; ++held)
            EXPECT_LT(foc::detail::NextFreeSlot(slotCount, ready, held), slotCount) << " ready=" << +ready << " held=" << +held;
}

TEST_F(TripleBufferSlotTest, a_reader_stalled_for_many_periods_keeps_its_slot)
{
    constexpr uint8_t held = 1;
    uint8_t ready = 0;
    uint8_t writeSlot = foc::detail::NextFreeSlot(slotCount, ready, held);

    for (std::size_t publication = 0; publication != 100; ++publication)
    {
        EXPECT_NE(held, writeSlot) << " at publication " << publication;
        writeSlot = Advance(ready, writeSlot, held);
    }
}

TEST_F(TripleBufferSlotTest, a_reader_that_keeps_up_still_never_collides_with_the_writer)
{
    uint8_t ready = 0;
    uint8_t held = 0;
    uint8_t writeSlot = foc::detail::NextFreeSlot(slotCount, ready, held);

    for (std::size_t publication = 0; publication != 100; ++publication)
    {
        EXPECT_NE(held, writeSlot) << " at publication " << publication;
        writeSlot = Advance(ready, writeSlot, held);
        held = ready;
    }
}
