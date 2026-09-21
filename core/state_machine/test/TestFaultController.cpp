#include "TestFocStateMachineHelper.hpp"
#include "core/state_machine/FaultController.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"

namespace
{
    using namespace testing;

    class FaultControllerTest
        : public ::testing::Test
        , public infra::ClockFixture
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

TEST_F(FaultControllerTest, latching_from_an_interrupt_sets_the_latch_and_records_the_code)
{
    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(controller.IsLatched());
    EXPECT_TRUE(controller.IsPending());
    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overcurrent);
}

TEST_F(FaultControllerTest, a_pending_fault_is_taken_once)
{
    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(controller.TakePendingFault().has_value());
    EXPECT_FALSE(controller.TakePendingFault().has_value());
}

TEST_F(FaultControllerTest, a_second_interrupt_replaces_the_code_the_pending_fault_carries)
{
    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);
    controller.LatchFromInterrupt(state_machine::FaultCode::overvoltage);

    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overvoltage);
}

TEST_F(FaultControllerTest, no_fault_is_pending_before_an_interrupt_latches_one)
{
    EXPECT_FALSE(controller.IsPending());
    EXPECT_FALSE(controller.TakePendingFault().has_value());
}

TEST_F(FaultControllerTest, only_taking_the_pending_fault_consumes_it)
{
    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overcurrent);

    controller.EnterFault();

    EXPECT_FALSE(controller.TakePendingFault().has_value());
    EXPECT_TRUE(controller.IsLatched());
}

TEST_F(FaultControllerTest, a_fault_latched_while_the_transition_runs_is_not_swallowed_by_it)
{
    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);
    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overcurrent);

    controller.LatchFromInterrupt(state_machine::FaultCode::overvoltage);
    controller.EnterFault();

    EXPECT_TRUE(controller.IsPending());
    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overvoltage);
}

TEST_F(FaultControllerTest, clearing_leaves_a_pending_fault_for_its_own_transition)
{
    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);
    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overcurrent);
    controller.EnterFault();

    controller.LatchFromInterrupt(state_machine::FaultCode::overvoltage);
    controller.Clear();

    EXPECT_TRUE(controller.IsPending());
    EXPECT_EQ(controller.TakePendingFault(), state_machine::FaultCode::overvoltage);
}

TEST_F(FaultControllerTest, entering_the_fault_marks_it_recorded_and_clearing_unmarks_it)
{
    EXPECT_FALSE(controller.IsRecorded());

    controller.LatchFromInterrupt(state_machine::FaultCode::overcurrent);
    EXPECT_FALSE(controller.IsRecorded());

    controller.EnterFault();
    EXPECT_TRUE(controller.IsRecorded());

    controller.Clear();
    EXPECT_FALSE(controller.IsRecorded());
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

namespace
{
    class FaultControllerConditionTest
        : public FaultControllerTest
    {
    public:
        void Condition(state_machine::FaultConditionState state)
        {
            EXPECT_CALL(notifier, ConditionState()).WillRepeatedly(Return(state));
            controller.SampleCondition();
        }

        void EnterFaultWithCondition(state_machine::FaultConditionState state)
        {
            controller.EnterFault();
            Condition(state);
        }
    };
}

TEST_F(FaultControllerConditionTest, a_clear_is_refused_while_the_condition_reads_asserted)
{
    RegisterWithCallbacks();
    EnterFaultWithCondition(state_machine::FaultConditionState::asserted);

    EXPECT_EQ(application::ClearRefusal::conditionAsserted, controller.EvaluateClear());
    EXPECT_FALSE(controller.TryClear());
    EXPECT_TRUE(controller.IsLatched());
}

TEST_F(FaultControllerConditionTest, a_clear_is_refused_until_the_dwell_has_elapsed_after_deassertion)
{
    RegisterWithCallbacks();
    EnterFaultWithCondition(state_machine::FaultConditionState::asserted);
    Condition(state_machine::FaultConditionState::clear);

    EXPECT_EQ(application::ClearRefusal::dwellNotElapsed, controller.EvaluateClear());

    ForwardTime(application::FaultController::conditionDwell);

    EXPECT_EQ(application::ClearRefusal::none, controller.EvaluateClear());
    EXPECT_TRUE(controller.TryClear());
}

TEST_F(FaultControllerConditionTest, an_asserted_sample_during_the_dwell_restarts_it)
{
    RegisterWithCallbacks();
    EnterFaultWithCondition(state_machine::FaultConditionState::asserted);
    Condition(state_machine::FaultConditionState::clear);

    ForwardTime(application::FaultController::conditionDwell - std::chrono::milliseconds{ 10 });
    Condition(state_machine::FaultConditionState::asserted);
    Condition(state_machine::FaultConditionState::clear);
    ForwardTime(application::FaultController::conditionDwell - std::chrono::milliseconds{ 10 });

    EXPECT_EQ(application::ClearRefusal::dwellNotElapsed, controller.EvaluateClear());

    ForwardTime(std::chrono::milliseconds{ 10 });

    EXPECT_EQ(application::ClearRefusal::none, controller.EvaluateClear());
}

TEST_F(FaultControllerConditionTest, a_clear_is_allowed_when_the_platform_cannot_interrogate_the_condition)
{
    RegisterWithCallbacks();
    EnterFaultWithCondition(state_machine::FaultConditionState::unknown);

    EXPECT_EQ(application::ClearRefusal::none, controller.EvaluateClear());
    EXPECT_TRUE(controller.TryClear());
}

TEST_F(FaultControllerConditionTest, the_retry_limit_still_refuses_a_clear_whose_condition_reads_clear)
{
    RegisterWithCallbacks();

    for (int i = 0; i != 3; ++i)
    {
        EnterFaultWithCondition(state_machine::FaultConditionState::asserted);
        Condition(state_machine::FaultConditionState::clear);
        ForwardTime(application::FaultController::conditionDwell);
        EXPECT_TRUE(controller.TryClear());
    }

    EnterFaultWithCondition(state_machine::FaultConditionState::asserted);
    Condition(state_machine::FaultConditionState::clear);
    ForwardTime(application::FaultController::conditionDwell);

    EXPECT_EQ(application::ClearRefusal::retryLimitReached, controller.EvaluateClear());
    EXPECT_FALSE(controller.TryClear());
}

TEST_F(FaultControllerConditionTest, the_condition_is_reported_before_the_retry_limit)
{
    RegisterWithCallbacks();

    for (int i = 0; i != 3; ++i)
    {
        controller.EnterFault();
        EXPECT_TRUE(controller.TryClear());
    }

    EnterFaultWithCondition(state_machine::FaultConditionState::asserted);

    EXPECT_EQ(application::ClearRefusal::conditionAsserted, controller.EvaluateClear());
}

TEST_F(FaultControllerConditionTest, a_controller_without_a_notifier_reports_the_condition_as_unknown)
{
    controller.EnterFault();
    controller.SampleCondition();

    EXPECT_EQ(state_machine::FaultConditionState::unknown, controller.ConditionState());
    EXPECT_EQ(application::ClearRefusal::none, controller.EvaluateClear());
}
