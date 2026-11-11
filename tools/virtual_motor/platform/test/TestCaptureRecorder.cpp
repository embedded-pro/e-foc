#include "tools/virtual_motor/platform/CaptureRecorder.hpp"
#include "tools/virtual_motor/platform/test/InstrumentReportsMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

namespace
{
    using namespace virtual_motor::port;

    MATCHER_P2(IsEvent, kind, period, "")
    {
        return arg.event == kind && arg.period == static_cast<uint64_t>(period);
    }

    Sample Numbered(uint64_t period)
    {
        Sample sample{};
        sample[0] = static_cast<float>(period);
        return sample;
    }

    class TestCaptureRecorder
        : public testing::Test
    {
    public:
        TestCaptureRecorder()
        {
            recorder.Report(reports);
        }

        void Arm(const CaptureRequest& request, uint64_t at)
        {
            EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::armed, at)));
            EXPECT_FALSE(recorder.Arm(request, at).has_value());
        }

        void Observe(uint64_t from, uint64_t to)
        {
            for (auto period = from; period != to; ++period)
                recorder.Observe(period, Numbered(period), 0, false);
        }

        void CollectChunks()
        {
            EXPECT_CALL(reports, ReportCaptureChunk(testing::_)).WillRepeatedly([this](const CaptureChunk& chunk)
                {
                    chunks.push_back(chunk);
                });
        }

        testing::StrictMock<virtual_motor::InstrumentReportsMock> reports;
        virtual_motor::CaptureRecorder recorder;
        std::vector<CaptureChunk> chunks;
    };
}

TEST_F(TestCaptureRecorder, an_immediate_capture_records_its_length_from_the_next_period)
{
    Arm(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 100 }, 50);
    EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::triggered, 50)));
    CollectChunks();

    Observe(50, 160);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0].length, 100);
    EXPECT_EQ(chunks[0].preTrigger, 0);
    EXPECT_EQ(chunks[0].firstPeriod, 50u);
    ASSERT_EQ(chunks[0].periods.size(), 100u);
    EXPECT_FLOAT_EQ(chunks[0].periods[99][0], 149.0f);
    EXPECT_FALSE(recorder.Active());
}

TEST_F(TestCaptureRecorder, keeps_its_pre_trigger_share_of_the_periods_before_the_trigger)
{
    Arm(CaptureRequest{ Trigger{ TriggerKind::rising, 0, 300.0f }, 25, 100 }, 0);
    EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::triggered, 300)));
    CollectChunks();

    Observe(0, 500);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0].preTrigger, 25);
    EXPECT_EQ(chunks[0].firstPeriod, 275u);
    EXPECT_FLOAT_EQ(chunks[0].periods[25][0], 300.0f);
    EXPECT_FLOAT_EQ(chunks[0].periods[99][0], 374.0f);
}

TEST_F(TestCaptureRecorder, a_trigger_that_fires_early_yields_fewer_periods_before_it)
{
    Arm(CaptureRequest{ Trigger{ TriggerKind::rising, 0, 10.0f }, 50, 100 }, 0);
    EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::triggered, 10)));
    CollectChunks();

    Observe(0, 200);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0].preTrigger, 10);
    EXPECT_EQ(chunks[0].length, 60);
    EXPECT_EQ(chunks[0].firstPeriod, 0u);
}

TEST_F(TestCaptureRecorder, a_long_capture_travels_in_chunks_of_at_most_512_periods)
{
    Arm(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 1300 }, 0);
    EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::triggered, 0)));
    CollectChunks();

    Observe(0, 1300);

    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[1].index, 512);
    EXPECT_EQ(chunks[2].index, 1024);
    EXPECT_EQ(chunks[2].periods.size(), 276u);
    EXPECT_FLOAT_EQ(chunks[2].periods[275][0], 1299.0f);
    EXPECT_EQ(chunks[0].capture, chunks[2].capture);
}

TEST_F(TestCaptureRecorder, cancelling_reports_it_and_records_nothing_more)
{
    Arm(CaptureRequest{ Trigger{ TriggerKind::rising, 0, 1000.0f }, 0, 10 }, 0);
    Observe(0, 5);

    EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::cancelled, 5)));
    recorder.Cancel(5);
    Observe(5, 2000);

    EXPECT_FALSE(recorder.Active());
}

TEST_F(TestCaptureRecorder, a_new_request_cancels_the_armed_capture_and_takes_the_next_number)
{
    Arm(CaptureRequest{ Trigger{ TriggerKind::rising, 0, 1000.0f }, 0, 10 }, 0);

    testing::InSequence sequence;
    EXPECT_CALL(reports, ReportCaptureEvent(IsEvent(CaptureEventKind::cancelled, 3)));
    EXPECT_CALL(reports, ReportCaptureEvent(testing::AllOf(IsEvent(CaptureEventKind::armed, 3), testing::Field(&CaptureEvent::capture, 2))));
    EXPECT_FALSE(recorder.Arm(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 10 }, 3).has_value());
}

TEST_F(TestCaptureRecorder, refuses_a_length_share_or_trigger_out_of_range)
{
    EXPECT_EQ(recorder.Arm(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 0 }, 0), RefusalReason::outOfRange);
    EXPECT_EQ(recorder.Arm(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 16385 }, 0), RefusalReason::outOfRange);
    EXPECT_EQ(recorder.Arm(CaptureRequest{ Trigger{ TriggerKind::immediately }, 101, 100 }, 0), RefusalReason::outOfRange);
    EXPECT_EQ(recorder.Arm(CaptureRequest{ Trigger{ TriggerKind::rising, 30, 0.0f }, 0, 100 }, 0), RefusalReason::outOfRange);
    EXPECT_FALSE(recorder.Active());
}
