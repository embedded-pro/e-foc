#include "core/state_machine/CommissioningCommands.hpp"
#include "gtest/gtest.h"

TEST(TestCommissioningCommands, a_refused_or_forbidden_event_is_a_rejected_command)
{
    EXPECT_EQ(state_machine::CommandResult::rejected, application::ToCommandResult(services::DispatchResult::rejected));
    EXPECT_EQ(state_machine::CommandResult::rejected, application::ToCommandResult(services::DispatchResult::forbidden));
}

TEST(TestCommissioningCommands, a_queued_event_is_a_queued_command)
{
    EXPECT_EQ(state_machine::CommandResult::queued, application::ToCommandResult(services::DispatchResult::queued));
}

TEST(TestCommissioningCommands, a_transition_is_an_accepted_command)
{
    EXPECT_EQ(state_machine::CommandResult::ok, application::ToCommandResult(services::DispatchResult::transitioned));
}
