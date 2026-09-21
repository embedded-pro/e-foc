#include "core/foc/model/PlantResponseRecorder.hpp"
#include <gtest/gtest.h>

namespace
{
    constexpr std::size_t kCapacity = 8;

    class PlantResponseRecorderTest
        : public ::testing::Test
    {
    protected:
        foc::PlantResponseSample At(uint32_t tick) const
        {
            return foc::PlantResponseSample{ tick, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        }

        std::vector<foc::PlantResponseRecord> Drain()
        {
            std::vector<foc::PlantResponseRecord> out;
            while (auto record = recorder.Pop())
                out.push_back(*record);
            return out;
        }

        foc::PlantResponseRecorder<kCapacity> recorder;
    };
}

TEST_F(PlantResponseRecorderTest, disabled_recorder_captures_nothing)
{
    recorder.Begin();
    recorder.Capture(At(1));
    recorder.Capture(At(2));

    EXPECT_FALSE(recorder.Pop().has_value());
}

TEST_F(PlantResponseRecorderTest, capture_before_begin_is_ignored)
{
    recorder.Configure(1, 0);
    recorder.Capture(At(1));

    EXPECT_FALSE(recorder.Pop().has_value());
}

TEST_F(PlantResponseRecorderTest, begin_emits_marker_then_samples_every_tick)
{
    recorder.Configure(1, 0);
    recorder.Begin();
    recorder.Capture(At(10));
    recorder.Capture(At(11));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(records[0].kind, foc::PlantResponseKind::began);
    EXPECT_EQ(records[0].sample.tick, 10u);
    EXPECT_EQ(records[1].kind, foc::PlantResponseKind::sample);
    EXPECT_EQ(records[1].sample.tick, 10u);
    EXPECT_EQ(records[2].sample.tick, 11u);
}

TEST_F(PlantResponseRecorderTest, decimation_keeps_one_sample_in_n)
{
    recorder.Configure(3, 0);
    recorder.Begin();
    for (uint32_t tick = 0; tick != 7; ++tick)
        recorder.Capture(At(tick));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 4u);
    EXPECT_EQ(records[1].sample.tick, 0u);
    EXPECT_EQ(records[2].sample.tick, 3u);
    EXPECT_EQ(records[3].sample.tick, 6u);
}

TEST_F(PlantResponseRecorderTest, max_samples_caps_emission)
{
    recorder.Configure(1, 2);
    recorder.Begin();
    for (uint32_t tick = 0; tick != 5; ++tick)
        recorder.Capture(At(tick));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(records[2].sample.tick, 1u);
}

TEST_F(PlantResponseRecorderTest, sample_carries_plant_values)
{
    recorder.Configure(1, 0);
    recorder.Begin();
    recorder.Capture(At(4));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_FLOAT_EQ(records[1].sample.omegaMech, 1.0f);
    EXPECT_FLOAT_EQ(records[1].sample.thetaMech, 2.0f);
    EXPECT_FLOAT_EQ(records[1].sample.iq, 3.0f);
    EXPECT_FLOAT_EQ(records[1].sample.id, 4.0f);
    EXPECT_FLOAT_EQ(records[1].sample.externalTorqueNm, 5.0f);
}

TEST_F(PlantResponseRecorderTest, note_is_ordered_between_samples)
{
    recorder.Configure(1, 0);
    recorder.Begin();
    recorder.Capture(At(0));
    recorder.Note(foc::PlantResponseKind::torqueStep, At(1));
    recorder.Capture(At(1));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 4u);
    EXPECT_EQ(records[2].kind, foc::PlantResponseKind::torqueStep);
    EXPECT_EQ(records[3].kind, foc::PlantResponseKind::sample);
}

TEST_F(PlantResponseRecorderTest, note_before_begin_is_ignored)
{
    recorder.Configure(1, 0);
    recorder.Note(foc::PlantResponseKind::torqueStep, At(1));

    EXPECT_FALSE(recorder.Pop().has_value());
}

TEST_F(PlantResponseRecorderTest, end_emits_stopped_marker_and_disarms)
{
    recorder.Configure(1, 0);
    recorder.Begin();
    recorder.Capture(At(0));
    recorder.End();
    recorder.Capture(At(1));
    recorder.Capture(At(2));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(records[2].kind, foc::PlantResponseKind::stopped);
    EXPECT_EQ(records[2].sample.tick, 1u);
    EXPECT_EQ(records[2].dropped, 0u);
}

TEST_F(PlantResponseRecorderTest, full_ring_counts_drops_and_reports_them_on_stop)
{
    recorder.Configure(1, 0);
    recorder.Begin();
    for (uint32_t tick = 0; tick != 2 * kCapacity; ++tick)
        recorder.Capture(At(tick));

    EXPECT_EQ(recorder.Dropped(), 2 * kCapacity - (kCapacity - 1));

    Drain();
    recorder.End();
    recorder.Capture(At(100));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].kind, foc::PlantResponseKind::stopped);
    EXPECT_EQ(records[0].dropped, recorder.Dropped());
}

TEST_F(PlantResponseRecorderTest, begin_again_restarts_the_cap_and_drop_count)
{
    recorder.Configure(1, 1);
    recorder.Begin();
    recorder.Capture(At(0));
    recorder.Capture(At(1));
    Drain();

    recorder.Begin();
    recorder.Capture(At(2));

    const auto records = Drain();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].kind, foc::PlantResponseKind::began);
    EXPECT_EQ(records[1].sample.tick, 2u);
}
