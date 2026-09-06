#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include <gtest/gtest.h>

namespace
{
    constexpr uint8_t slotCount = 3;

    class EstimatorChannelSlotTest
        : public ::testing::Test
    {
    public:
        // Walks the writer forward the way Publish does: publish into the slot it holds, that slot
        // becomes the one last published, then pick the next free one.
        static uint8_t Advance(uint8_t& ready, uint8_t writeSlot, uint8_t held)
        {
            ready = writeSlot;
            return foc::detail::NextFreeSlot(slotCount, ready, held);
        }
    };
}

TEST_F(EstimatorChannelSlotTest, the_writer_never_lands_on_the_slot_it_just_published)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
        for (uint8_t held = 0; held != slotCount; ++held)
            EXPECT_NE(ready, foc::detail::NextFreeSlot(slotCount, ready, held)) << " ready=" << +ready << " held=" << +held;
}

TEST_F(EstimatorChannelSlotTest, the_writer_never_lands_on_the_slot_the_reader_holds)
{
    // With ready == held the reader is on the slot last published and both remaining slots are
    // free, so only the distinct case constrains the choice.
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

TEST_F(EstimatorChannelSlotTest, the_chosen_slot_is_always_in_range)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
        for (uint8_t held = 0; held != slotCount; ++held)
            EXPECT_LT(foc::detail::NextFreeSlot(slotCount, ready, held), slotCount) << " ready=" << +ready << " held=" << +held;
}

TEST_F(EstimatorChannelSlotTest, a_reader_stalled_for_many_periods_keeps_its_slot)
{
    // The defect two slots had: a low-priority handler that overruns its period sees the writer
    // wrap back onto the slot it is still reading from. Here the reader claims one slot and never
    // releases it while the writer publishes far more times than there are slots.
    constexpr uint8_t held = 1;
    uint8_t ready = 0;
    uint8_t writeSlot = foc::detail::NextFreeSlot(slotCount, ready, held);

    for (std::size_t publication = 0; publication != 100; ++publication)
    {
        EXPECT_NE(held, writeSlot) << " at publication " << publication;
        writeSlot = Advance(ready, writeSlot, held);
    }
}

TEST_F(EstimatorChannelSlotTest, a_reader_that_keeps_up_still_never_collides_with_the_writer)
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
