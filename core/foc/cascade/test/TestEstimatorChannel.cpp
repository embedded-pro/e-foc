#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include <gtest/gtest.h>

namespace
{
    constexpr uint8_t slotCount = 3;

    class EstimatorChannelSlotTest
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

TEST_F(EstimatorChannelSlotTest, the_writer_never_lands_on_the_slot_it_just_published)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
        for (uint8_t held = 0; held != slotCount; ++held)
            EXPECT_NE(ready, foc::detail::NextFreeSlot(slotCount, ready, held)) << " ready=" << +ready << " held=" << +held;
}

TEST_F(EstimatorChannelSlotTest, the_writer_never_lands_on_the_slot_the_reader_holds)
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

TEST_F(EstimatorChannelSlotTest, the_chosen_slot_is_always_in_range)
{
    for (uint8_t ready = 0; ready != slotCount; ++ready)
        for (uint8_t held = 0; held != slotCount; ++held)
            EXPECT_LT(foc::detail::NextFreeSlot(slotCount, ready, held), slotCount) << " ready=" << +ready << " held=" << +held;
}

TEST_F(EstimatorChannelSlotTest, a_reader_stalled_for_many_periods_keeps_its_slot)
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

namespace
{
    class EstimatorWindowAccumulatorTest
        : public ::testing::Test
    {
    public:
        static float MeanDirectVoltageOverRamp(uint32_t dutyUpdateDelayPeriods)
        {
            constexpr uint32_t ticks = 4;
            foc::EstimatorWindowAccumulator accumulator{ hal::Hertz{ 20000 }, ticks, dutyUpdateDelayPeriods };

            for (uint32_t tick = 0; tick != ticks; ++tick)
                accumulator.Accumulate(foc::RotatingFrame{ 0.0f, 0.0f }, foc::RotatingFrame{ static_cast<float>(tick + 1), 0.0f }, 0.0f, 4.0f);

            return accumulator.Close().meanNormalizedVd;
        }
    };
}

TEST_F(EstimatorWindowAccumulatorTest, a_duty_that_acts_at_once_is_paired_with_the_current_step_that_follows_it)
{
    EXPECT_FLOAT_EQ(MeanDirectVoltageOverRamp(0), (0.0f + 1.0f + 2.0f + 3.0f) / 4.0f);
}

TEST_F(EstimatorWindowAccumulatorTest, a_duty_that_acts_a_period_late_is_paired_with_the_current_step_a_period_later)
{
    EXPECT_FLOAT_EQ(MeanDirectVoltageOverRamp(1), (0.0f + 0.0f + 1.0f + 2.0f) / 4.0f);
}
