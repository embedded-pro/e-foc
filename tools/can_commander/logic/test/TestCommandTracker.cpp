#include "core/can/FocMotorMessages.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/logic/CommandTracker.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using testing::StrictMock;
    using tool::CommandTracker;

    class TestCommandTracker
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<testing::MockFunction<void(uint16_t, bool)>> busyChanged;
        StrictMock<testing::MockFunction<void(uint16_t, uint8_t)>> timedOut;
        CommandTracker tracker{ [this](uint16_t nodeId, bool busy)
            {
                busyChanged.Call(nodeId, busy);
            },
            [this](uint16_t nodeId, uint8_t commandType)
            {
                timedOut.Call(nodeId, commandType);
            } };
    };
}

TEST_F(TestCommandTracker, a_node_without_commands_is_not_busy)
{
    EXPECT_FALSE(tracker.IsBusy(3));
}

TEST_F(TestCommandTracker, marking_a_command_reports_the_node_busy_once)
{
    EXPECT_CALL(busyChanged, Call(3, true));
    tracker.MarkBusy(3, can::focStopId);
    tracker.MarkBusy(3, can::focStartId);

    EXPECT_TRUE(tracker.IsBusy(3));
    EXPECT_FALSE(tracker.IsBusy(4));
}

TEST_F(TestCommandTracker, completing_the_pending_command_clears_the_busy_state)
{
    EXPECT_CALL(busyChanged, Call(3, true));
    tracker.MarkBusy(3, can::focStopId);

    tracker.Complete(3, can::focStartId);
    EXPECT_TRUE(tracker.IsBusy(3));

    EXPECT_CALL(busyChanged, Call(3, false));
    tracker.Complete(3, can::focStopId);
    EXPECT_FALSE(tracker.IsBusy(3));
}

TEST_F(TestCommandTracker, completing_or_clearing_an_idle_node_reports_nothing)
{
    tracker.Complete(3, can::focStopId);
    tracker.Clear(3);

    EXPECT_FALSE(tracker.IsBusy(3));
}

TEST_F(TestCommandTracker, an_unanswered_command_times_out_after_its_own_timeout)
{
    EXPECT_CALL(busyChanged, Call(3, true));
    tracker.MarkBusy(3, can::focStopId);

    ForwardTime(CommandTracker::commandTimeout - std::chrono::milliseconds(1));
    testing::Mock::VerifyAndClearExpectations(&timedOut);

    EXPECT_CALL(busyChanged, Call(3, false));
    EXPECT_CALL(timedOut, Call(3, can::focStopId));
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_FALSE(tracker.IsBusy(3));
}

TEST_F(TestCommandTracker, clear_all_releases_every_busy_node)
{
    EXPECT_CALL(busyChanged, Call(3, true));
    EXPECT_CALL(busyChanged, Call(4, true));
    tracker.MarkBusy(3, can::focStopId);
    tracker.MarkBusy(4, can::focStopId);

    EXPECT_CALL(busyChanged, Call(3, false));
    EXPECT_CALL(busyChanged, Call(4, false));
    tracker.ClearAll();

    ForwardTime(CommandTracker::commandTimeout);
}

TEST_F(TestCommandTracker, motion_and_start_commands_get_longer_timeouts)
{
    EXPECT_EQ(CommandTracker::motionCommandTimeout, CommandTracker::TimeoutFor(can::focAlignId));
    EXPECT_EQ(CommandTracker::motionCommandTimeout, CommandTracker::TimeoutFor(can::focIdentifyElectricalId));
    EXPECT_EQ(CommandTracker::motionCommandTimeout, CommandTracker::TimeoutFor(can::focCalibrateId));
    EXPECT_EQ(CommandTracker::startCommandTimeout, CommandTracker::TimeoutFor(can::focStartId));
    EXPECT_EQ(CommandTracker::commandTimeout, CommandTracker::TimeoutFor(can::focStopId));
}
