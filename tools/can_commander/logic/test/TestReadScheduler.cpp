#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/logic/ReadScheduler.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using testing::Return;
    using testing::StrictMock;
    using tool::NodeRead;
    using tool::ReadScheduler;

    const NodeRead algorithms{ can::focQueryAlgorithmsId };
    const NodeRead bandwidth{ can::focQueryParameterId, can::FocParameter::currentBandwidth };
    const NodeRead telemetry{ can::focRequestTelemetryId };

    class TestReadScheduler
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<testing::MockFunction<bool(uint16_t, const NodeRead&)>> send;
        ReadScheduler scheduler{ [this](uint16_t nodeId, const NodeRead& read)
            {
                return send.Call(nodeId, read);
            } };
    };
}

TEST_F(TestReadScheduler, the_first_read_goes_out_at_once_and_the_next_waits_for_its_answer)
{
    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);

    EXPECT_CALL(send, Call(3, bandwidth)).WillOnce(Return(true));
    scheduler.Complete(3, can::focQueryAlgorithmsId);
}

TEST_F(TestReadScheduler, an_answer_to_another_read_does_not_release_the_queue)
{
    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);

    scheduler.Complete(3, can::focQueryParameterId);
    scheduler.Complete(4, can::focQueryAlgorithmsId);
}

TEST_F(TestReadScheduler, a_read_already_waiting_is_queued_once)
{
    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);
    scheduler.Queue(3, bandwidth);

    EXPECT_CALL(send, Call(3, bandwidth)).WillOnce(Return(true));
    scheduler.Complete(3, can::focQueryAlgorithmsId);
    scheduler.Complete(3, can::focQueryParameterId);
}

TEST_F(TestReadScheduler, a_read_that_cannot_be_sent_is_skipped)
{
    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);
    scheduler.Queue(3, telemetry);

    EXPECT_CALL(send, Call(3, bandwidth)).WillOnce(Return(false));
    EXPECT_CALL(send, Call(3, telemetry)).WillOnce(Return(true));
    scheduler.Complete(3, can::focQueryAlgorithmsId);
}

TEST_F(TestReadScheduler, an_unanswered_read_times_out_and_the_next_goes_out)
{
    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);

    EXPECT_CALL(send, Call(3, bandwidth)).WillOnce(Return(true));
    ForwardTime(ReadScheduler::readTimeout);
}

TEST_F(TestReadScheduler, the_in_flight_parameter_is_known_only_for_its_command)
{
    EXPECT_FALSE(scheduler.InFlightParameter(3, can::focQueryParameterId).has_value());

    EXPECT_CALL(send, Call(3, bandwidth)).WillOnce(Return(true));
    scheduler.Queue(3, bandwidth);

    EXPECT_EQ(can::FocParameter::currentBandwidth, scheduler.InFlightParameter(3, can::focQueryParameterId));
    EXPECT_FALSE(scheduler.InFlightParameter(3, can::focQueryAlgorithmsId).has_value());
}

TEST_F(TestReadScheduler, queued_reads_can_be_searched)
{
    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);

    const auto isBandwidth = [](const NodeRead& read)
    {
        return read.parameter == can::FocParameter::currentBandwidth && read.commandType == can::focQueryParameterId;
    };
    EXPECT_TRUE(scheduler.AnyQueued(3, isBandwidth));
    EXPECT_FALSE(scheduler.AnyQueued(4, isBandwidth));
}

TEST_F(TestReadScheduler, forgetting_a_node_drops_its_reads)
{
    scheduler.Forget(3);

    EXPECT_CALL(send, Call(3, algorithms)).WillOnce(Return(true));
    scheduler.Queue(3, algorithms);
    scheduler.Queue(3, bandwidth);
    scheduler.Forget(3);

    scheduler.Complete(3, can::focQueryAlgorithmsId);
    ForwardTime(ReadScheduler::readTimeout);

    EXPECT_CALL(send, Call(3, telemetry)).WillOnce(Return(true));
    scheduler.Queue(3, telemetry);
}

TEST_F(TestReadScheduler, only_queries_and_telemetry_requests_are_reads)
{
    EXPECT_TRUE(ReadScheduler::IsRead(can::focQueryAlgorithmsId));
    EXPECT_TRUE(ReadScheduler::IsRead(can::focQueryParameterId));
    EXPECT_TRUE(ReadScheduler::IsRead(can::focRequestTelemetryId));
    EXPECT_TRUE(ReadScheduler::IsRead(can::focQueryFaultHistoryId));
    EXPECT_FALSE(ReadScheduler::IsRead(can::focStartId));
}
