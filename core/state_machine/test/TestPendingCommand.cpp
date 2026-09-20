#include "core/state_machine/PendingCommand.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    class PendingCommandTest
        : public testing::Test
    {
    public:
        state_machine::CommandCallback Callback()
        {
            return state_machine::CommandCallback{ [this](state_machine::CommandResult result)
                {
                    onDone.callback(result);
                } };
        }

        testing::StrictMock<infra::MockCallback<void(state_machine::CommandResult)>> onDone;
        application::PendingCommand pending;
    };
}

TEST_F(PendingCommandTest, nothing_is_pending_initially)
{
    EXPECT_FALSE(pending.Pending());

    pending.Complete(state_machine::CommandResult::ok);
    pending.FlushDeferred();
}

TEST_F(PendingCommandTest, accepted_command_is_pending_until_completed)
{
    pending.Accept(Callback());
    EXPECT_TRUE(pending.Pending());

    EXPECT_CALL(onDone, callback(state_machine::CommandResult::calibrationFailed));
    pending.Complete(state_machine::CommandResult::calibrationFailed);

    EXPECT_FALSE(pending.Pending());
}

TEST_F(PendingCommandTest, deferred_result_is_delivered_on_flush_only)
{
    pending.Accept(Callback());
    pending.CompleteAfterTransition(state_machine::CommandResult::abortedByFault);
    EXPECT_TRUE(pending.Pending());

    EXPECT_CALL(onDone, callback(state_machine::CommandResult::abortedByFault));
    pending.FlushDeferred();

    EXPECT_FALSE(pending.Pending());
    pending.FlushDeferred();
}
