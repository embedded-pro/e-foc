#include "TestFocStateMachineHelper.hpp"
#include "core/state_machine/FaultController.hpp"

namespace
{
    using namespace testing;

    class FaultControllerTest
        : public ::testing::Test
    {
    public:
        StrictMock<state_machine::FaultNotifierMock> notifier;
        application::FaultController controller;

        infra::Function<void(state_machine::FaultCode)> storedImmediate;
        infra::Function<void(state_machine::FaultCode)> storedDeferred;

        void RegisterWithCallbacks()
        {
            EXPECT_CALL(notifier, Register(_, _))
                .WillOnce(Invoke([this](
                    const infra::Function<void(state_machine::FaultCode)>& imm,
                    const infra::Function<void(state_machine::FaultCode)>& def)
                    {
                        storedImmediate = imm;
                        storedDeferred = def;
                    }));
            controller.Register(
                notifier,
                [](state_machine::FaultCode) {},
                [](state_machine::FaultCode) {});
        }
    };
}

TEST_F(FaultControllerTest, is_latched_false_initially)
{
    EXPECT_FALSE(controller.IsLatched());
}

TEST_F(FaultControllerTest, register_calls_notifier_register)
{
    EXPECT_CALL(notifier, Register(_, _));
    controller.Register(notifier, [](state_machine::FaultCode) {}, [](state_machine::FaultCode) {});
}

TEST_F(FaultControllerTest, unregister_calls_notifier_unregister)
{
    EXPECT_CALL(notifier, Register(_, _));
    EXPECT_CALL(notifier, Unregister());
    controller.Register(notifier, [](state_machine::FaultCode) {}, [](state_machine::FaultCode) {});
    controller.Unregister();
}

TEST_F(FaultControllerTest, unregister_when_not_registered_does_nothing)
{
    controller.Unregister();
}

TEST_F(FaultControllerTest, enter_fault_sets_latched)
{
    controller.EnterFault();
    EXPECT_TRUE(controller.IsLatched());
}

TEST_F(FaultControllerTest, try_clear_returns_true_and_clears_latch)
{
    controller.EnterFault();
    EXPECT_TRUE(controller.TryClear());
    EXPECT_FALSE(controller.IsLatched());
}

TEST_F(FaultControllerTest, try_clear_returns_false_after_max_retries)
{
    for (int i = 0; i < 3; ++i)
    {
        controller.EnterFault();
        EXPECT_TRUE(controller.TryClear());
    }
    controller.EnterFault();
    EXPECT_FALSE(controller.TryClear());
    EXPECT_TRUE(controller.IsLatched());
}

TEST_F(FaultControllerTest, reset_clear_count_allows_clearing_again)
{
    for (int i = 0; i < 3; ++i)
    {
        controller.EnterFault();
        controller.TryClear();
    }
    controller.ResetClearCount();
    controller.EnterFault();
    EXPECT_TRUE(controller.TryClear());
}

TEST_F(FaultControllerTest, registered_callbacks_are_forwarded_to_notifier)
{
    bool immediateFired{ false };
    bool deferredFired{ false };

    EXPECT_CALL(notifier, Register(_, _))
        .WillOnce(Invoke([](
            const infra::Function<void(state_machine::FaultCode)>& imm,
            const infra::Function<void(state_machine::FaultCode)>& def)
            {
                imm(state_machine::FaultCode::overcurrent);
                def(state_machine::FaultCode::overcurrent);
            }));

    controller.Register(
        notifier,
        [&](state_machine::FaultCode) { immediateFired = true; },
        [&](state_machine::FaultCode) { deferredFired = true; });

    EXPECT_TRUE(immediateFired);
    EXPECT_TRUE(deferredFired);
}
