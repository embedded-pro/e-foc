#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestFaultNotifier
        : public ::testing::Test
    {
    };

    class TestFocStateMachineBase
        : public ::testing::Test
    {
    };

    // Minimal concrete derivation that implements pure virtuals but leaves
    // ApplyOnlineEstimates() at the default base-class body.
    class MinimalStateMachine
        : public state_machine::FocStateMachineBase
    {
    public:
        const state_machine::State& CurrentState() const override
        {
            return state;
        }

        state_machine::FaultCode LastFaultCode() const override
        {
            return state_machine::FaultCode::hardwareFault;
        }

        void CmdCalibrate() override {}
        void CmdEnable() override {}
        void CmdDisable() override {}
        void CmdClearFault() override {}
        void CmdClearCalibration() override {}

    private:
        state_machine::State state{ state_machine::Idle{} };
    };
}

TEST_F(TestFaultNotifier, no_op_fault_notifier_register_does_not_crash)
{
    state_machine::NoOpFaultNotifier notifier;
    bool called = false;
    notifier.Register([&called](state_machine::FaultCode)
        {
            called = true;
        });
    EXPECT_FALSE(called);
}

TEST_F(TestFaultNotifier, no_op_fault_notifier_register_accepts_empty_function)
{
    state_machine::NoOpFaultNotifier notifier;
    notifier.Register(infra::Function<void(state_machine::FaultCode)>{});
}

TEST_F(TestFocStateMachineBase, apply_online_estimates_default_implementation_does_not_crash)
{
    MinimalStateMachine sm;
    sm.ApplyOnlineEstimates();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(TestFocStateMachineBase, state_variants_are_constructible)
{
    state_machine::Idle idle{};
    state_machine::Calibrating calibrating{};
    state_machine::Ready ready{};
    state_machine::Enabled enabled{};
    state_machine::Fault fault{};

    EXPECT_EQ(calibrating.step, state_machine::CalibrationStep::polePairs);
    EXPECT_EQ(fault.code, state_machine::FaultCode::hardwareFault);
}
