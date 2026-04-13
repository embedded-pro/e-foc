#include "core/foc/implementations/LowPriorityInterruptImpl.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestLowPriorityInterruptImpl
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    protected:
        foc::LowPriorityInterruptImpl lowPriorityInterrupt;
    };
}

TEST_F(TestLowPriorityInterruptImpl, trigger_without_registration_does_not_crash)
{
    lowPriorityInterrupt.Trigger();
    ExecuteAllActions();
}

TEST_F(TestLowPriorityInterruptImpl, register_and_trigger_invokes_handler)
{
    bool handlerCalled = false;

    lowPriorityInterrupt.Register([&handlerCalled]()
        {
            handlerCalled = true;
        });

    lowPriorityInterrupt.Trigger();
    ExecuteAllActions();

    EXPECT_TRUE(handlerCalled);
}

TEST_F(TestLowPriorityInterruptImpl, register_overwrites_previous_handler)
{
    bool firstCalled = false;
    bool secondCalled = false;

    lowPriorityInterrupt.Register([&firstCalled]()
        {
            firstCalled = true;
        });

    lowPriorityInterrupt.Register([&secondCalled]()
        {
            secondCalled = true;
        });

    lowPriorityInterrupt.Trigger();
    ExecuteAllActions();

    EXPECT_FALSE(firstCalled);
    EXPECT_TRUE(secondCalled);
}

TEST_F(TestLowPriorityInterruptImpl, trigger_without_executing_does_not_call_handler_synchronously)
{
    bool handlerCalled = false;

    lowPriorityInterrupt.Register([&handlerCalled]()
        {
            handlerCalled = true;
        });

    lowPriorityInterrupt.Trigger();

    EXPECT_FALSE(handlerCalled);

    ExecuteAllActions();

    EXPECT_TRUE(handlerCalled);
}
