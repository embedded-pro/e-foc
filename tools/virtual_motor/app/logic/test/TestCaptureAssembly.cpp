#include "tools/virtual_motor/app/logic/CaptureAssembly.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace virtual_motor;

    port::CaptureEvent Event(uint16_t capture, port::CaptureEventKind kind, uint64_t period = 0)
    {
        return { capture, kind, period };
    }

    port::CaptureChunk Chunk(uint16_t capture, uint16_t length, uint16_t index, uint16_t count)
    {
        port::CaptureChunk chunk{ capture, length, 2, 100, index, {} };
        for (uint16_t offset = 0; offset != count; ++offset)
        {
            port::Sample sample{};
            sample[0] = static_cast<float>(index + offset);
            chunk.periods.push_back(sample);
        }
        return chunk;
    }
}

TEST(TestCaptureAssembly, follows_a_capture_from_armed_to_complete)
{
    app::CaptureAssembly assembly;
    EXPECT_EQ(assembly.State(), app::CaptureState::idle);

    assembly.Event(Event(3, port::CaptureEventKind::armed));
    EXPECT_EQ(assembly.State(), app::CaptureState::armed);
    assembly.Event(Event(3, port::CaptureEventKind::triggered, 102));
    EXPECT_EQ(assembly.State(), app::CaptureState::triggered);

    EXPECT_FALSE(assembly.Chunk(Chunk(3, 5, 0, 3)));
    EXPECT_EQ(assembly.State(), app::CaptureState::receiving);
    EXPECT_EQ(assembly.Received(), 3u);
    EXPECT_EQ(assembly.Expected(), 5u);
    EXPECT_FALSE(assembly.Last().has_value());

    EXPECT_TRUE(assembly.Chunk(Chunk(3, 5, 3, 2)));
    EXPECT_EQ(assembly.State(), app::CaptureState::complete);
    ASSERT_TRUE(assembly.Last().has_value());
    const auto& capture = *assembly.Last();
    EXPECT_EQ(capture.number, 3u);
    EXPECT_EQ(capture.preTrigger, 2u);
    EXPECT_EQ(capture.firstPeriod, 100u);
    ASSERT_EQ(capture.periods.size(), 5u);
    for (std::size_t index = 0; index != 5; ++index)
        EXPECT_NEAR(capture.periods[index][0], static_cast<float>(index), 1e-6f);
}

TEST(TestCaptureAssembly, chunks_are_placed_by_their_index_whatever_their_order)
{
    app::CaptureAssembly assembly;
    assembly.Event(Event(1, port::CaptureEventKind::armed));
    EXPECT_FALSE(assembly.Chunk(Chunk(1, 4, 2, 2)));
    EXPECT_TRUE(assembly.Chunk(Chunk(1, 4, 0, 2)));

    for (std::size_t index = 0; index != 4; ++index)
        EXPECT_NEAR(assembly.Last()->periods[index][0], static_cast<float>(index), 1e-6f);
}

TEST(TestCaptureAssembly, chunks_of_another_capture_are_ignored)
{
    app::CaptureAssembly assembly;
    assembly.Event(Event(2, port::CaptureEventKind::armed));

    EXPECT_FALSE(assembly.Chunk(Chunk(1, 2, 0, 2)));
    EXPECT_EQ(assembly.State(), app::CaptureState::armed);
    EXPECT_EQ(assembly.Received(), 0u);
}

TEST(TestCaptureAssembly, a_cancelled_or_interrupted_capture_keeps_the_last_complete_one)
{
    app::CaptureAssembly assembly;
    assembly.Event(Event(1, port::CaptureEventKind::armed));
    assembly.Chunk(Chunk(1, 2, 0, 2));

    assembly.Event(Event(2, port::CaptureEventKind::armed));
    assembly.Event(Event(2, port::CaptureEventKind::cancelled));
    EXPECT_EQ(assembly.State(), app::CaptureState::cancelled);
    EXPECT_EQ(assembly.Last()->number, 1u);

    assembly.Event(Event(3, port::CaptureEventKind::armed));
    assembly.Interrupted();
    EXPECT_EQ(assembly.State(), app::CaptureState::cancelled);
    EXPECT_EQ(assembly.Last()->number, 1u);

    assembly.Interrupted();
    EXPECT_EQ(assembly.State(), app::CaptureState::cancelled);
}

TEST(TestCaptureAssembly, interrupting_with_nothing_in_flight_changes_nothing)
{
    app::CaptureAssembly assembly;
    assembly.Interrupted();
    EXPECT_EQ(assembly.State(), app::CaptureState::idle);
}

TEST(TestCaptureAssembly, the_state_reads_in_plain_words)
{
    app::CaptureAssembly assembly;
    EXPECT_EQ(assembly.StatusText(), "No capture");
    assembly.Event(Event(1, port::CaptureEventKind::armed));
    EXPECT_EQ(assembly.StatusText(), "Armed…");
    assembly.Event(Event(1, port::CaptureEventKind::triggered));
    EXPECT_EQ(assembly.StatusText(), "Triggered");
    assembly.Chunk(Chunk(1, 600, 0, 2));
    EXPECT_EQ(assembly.StatusText(), "Receiving 2 of 600 periods");
    assembly.Chunk(Chunk(1, 600, 2, 598));
    EXPECT_EQ(assembly.StatusText(), "Complete: 600 periods");
    assembly.Event(Event(2, port::CaptureEventKind::armed));
    assembly.Event(Event(2, port::CaptureEventKind::cancelled));
    EXPECT_EQ(assembly.StatusText(), "Cancelled");
}

TEST(TestCaptureAssembly, a_period_received_twice_is_counted_once)
{
    app::CaptureAssembly assembly;
    assembly.Event(Event(1, port::CaptureEventKind::armed));

    EXPECT_FALSE(assembly.Chunk(Chunk(1, 3, 0, 2)));
    EXPECT_FALSE(assembly.Chunk(Chunk(1, 3, 1, 1)));
    EXPECT_EQ(assembly.Received(), 2u);
    EXPECT_TRUE(assembly.Chunk(Chunk(1, 3, 2, 1)));
}
