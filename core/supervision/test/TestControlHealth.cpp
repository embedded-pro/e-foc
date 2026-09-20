#include "core/supervision/ControlHealth.hpp"
#include <gtest/gtest.h>

namespace
{
    using state_machine::ControlMode;

    class ControlHealthTest
        : public testing::Test
    {
    public:
        state_machine::State Stopped() const
        {
            return state_machine::Idle{};
        }

        state_machine::State Enabled() const
        {
            return state_machine::Enabled{};
        }

        void SignalInnerLoop()
        {
            health.InnerLoopProgress().Signal();
        }

        void SignalOuterLoop()
        {
            health.OuterLoopProgress().Signal();
        }

        supervision::ControlHealth health;
    };
}

TEST_F(ControlHealthTest, stopped_and_calibrating_states_expect_no_control_progress)
{
    for (auto mode : { ControlMode::torque, ControlMode::speed, ControlMode::position })
        for (const auto& state : { state_machine::State{ state_machine::Idle{} },
                 state_machine::State{ state_machine::Ready{} },
                 state_machine::State{ state_machine::Calibrating{} },
                 state_machine::State{ state_machine::Fault{} } })
        {
            const auto expected = supervision::ControlHealth::Expected(mode, state);

            EXPECT_FALSE(expected.innerLoop);
            EXPECT_FALSE(expected.outerLoop);
        }
}

TEST_F(ControlHealthTest, enabled_torque_expects_the_inner_loop_only)
{
    const auto expected = supervision::ControlHealth::Expected(ControlMode::torque, Enabled());

    EXPECT_TRUE(expected.innerLoop);
    EXPECT_FALSE(expected.outerLoop);
}

TEST_F(ControlHealthTest, enabled_speed_and_position_expect_both_loops)
{
    for (auto mode : { ControlMode::speed, ControlMode::position })
    {
        const auto expected = supervision::ControlHealth::Expected(mode, Enabled());

        EXPECT_TRUE(expected.innerLoop);
        EXPECT_TRUE(expected.outerLoop);
    }
}

TEST_F(ControlHealthTest, a_stopped_drive_is_eligible_without_any_progress)
{
    EXPECT_TRUE(health.TakeEligibility(ControlMode::position, Stopped()));
}

TEST_F(ControlHealthTest, enabled_torque_is_eligible_on_inner_loop_progress_alone)
{
    SignalInnerLoop();

    EXPECT_TRUE(health.TakeEligibility(ControlMode::torque, Enabled()));
}

TEST_F(ControlHealthTest, enabled_torque_is_not_eligible_without_inner_loop_progress)
{
    SignalOuterLoop();

    EXPECT_FALSE(health.TakeEligibility(ControlMode::torque, Enabled()));
}

TEST_F(ControlHealthTest, enabled_speed_is_not_eligible_on_inner_loop_progress_alone)
{
    SignalInnerLoop();

    EXPECT_FALSE(health.TakeEligibility(ControlMode::speed, Enabled()));
}

TEST_F(ControlHealthTest, enabled_speed_is_not_eligible_on_outer_loop_progress_alone)
{
    SignalOuterLoop();

    EXPECT_FALSE(health.TakeEligibility(ControlMode::speed, Enabled()));
}

TEST_F(ControlHealthTest, enabled_speed_is_eligible_when_both_loops_progress)
{
    SignalInnerLoop();
    SignalOuterLoop();

    EXPECT_TRUE(health.TakeEligibility(ControlMode::speed, Enabled()));
}

TEST_F(ControlHealthTest, progress_is_consumed_by_the_evaluation_that_observes_it)
{
    SignalInnerLoop();
    ASSERT_TRUE(health.TakeEligibility(ControlMode::torque, Enabled()));

    EXPECT_FALSE(health.TakeEligibility(ControlMode::torque, Enabled()));
}

TEST_F(ControlHealthTest, progress_made_while_stopped_does_not_carry_into_enabled)
{
    SignalInnerLoop();
    SignalOuterLoop();
    ASSERT_TRUE(health.TakeEligibility(ControlMode::speed, Stopped()));

    EXPECT_FALSE(health.TakeEligibility(ControlMode::speed, Enabled()));
}

TEST_F(ControlHealthTest, switching_to_torque_stops_requiring_the_outer_loop)
{
    SignalInnerLoop();
    ASSERT_FALSE(health.TakeEligibility(ControlMode::speed, Enabled()));

    SignalInnerLoop();
    EXPECT_TRUE(health.TakeEligibility(ControlMode::torque, Enabled()));
}
