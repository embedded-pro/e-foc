#include "tools/virtual_motor/platform/RunLoop.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace std::chrono_literals;
    using virtual_motor::port::RefusalReason;
    using virtual_motor::port::StepUnit;

    class StepperMock
        : public virtual_motor::Stepper
    {
    public:
        MOCK_METHOD(void, AdvanceOnePeriod, (), (override));
        MOCK_METHOD(void, AdvanceOneMillisecond, (), (override));
        MOCK_METHOD(void, Mute, (bool muted), (override));
    };

    class TestRunLoop
        : public testing::Test
    {
    public:
        void Pause()
        {
            EXPECT_CALL(stepper, Mute(true));
            loop.Pause();
        }

        virtual_motor::Pacer::TimePoint now{};
        testing::StrictMock<StepperMock> stepper;
        virtual_motor::RunLoop loop{ stepper, [this]()
            {
                return now;
            } };
    };
}

TEST_F(TestRunLoop, runs_one_millisecond_per_elapsed_millisecond)
{
    now += 2ms;
    EXPECT_CALL(stepper, AdvanceOneMillisecond()).Times(2);
    loop.Service();

    EXPECT_FALSE(loop.Paused());
    EXPECT_EQ(loop.NextWake(), virtual_motor::Pacer::TimePoint{ 3ms });
}

TEST_F(TestRunLoop, a_paused_motor_stops_virtual_time_and_its_bus_traffic)
{
    Pause();

    now += 5ms;
    loop.Service();

    EXPECT_TRUE(loop.Paused());
    EXPECT_EQ(loop.NextWake(), now + 1ms);
}

TEST_F(TestRunLoop, resuming_continues_without_catching_up)
{
    Pause();
    now += 200ms;

    EXPECT_CALL(stepper, Mute(false));
    loop.Resume();
    loop.Service();
    EXPECT_EQ(loop.Overruns(), 0u);

    now += 1ms;
    EXPECT_CALL(stepper, AdvanceOneMillisecond());
    loop.Service();
}

TEST_F(TestRunLoop, pausing_or_resuming_twice_changes_nothing)
{
    Pause();
    loop.Pause();

    EXPECT_CALL(stepper, Mute(false));
    loop.Resume();
    loop.Resume();
}

TEST_F(TestRunLoop, a_paused_motor_steps_by_control_periods_or_milliseconds)
{
    Pause();

    EXPECT_CALL(stepper, AdvanceOnePeriod()).Times(3);
    EXPECT_FALSE(loop.Step(StepUnit::controlPeriod, 3).has_value());

    EXPECT_CALL(stepper, AdvanceOneMillisecond()).Times(2);
    EXPECT_FALSE(loop.Step(StepUnit::millisecond, 2).has_value());
}

TEST_F(TestRunLoop, refuses_a_step_while_running_or_in_an_unknown_unit)
{
    EXPECT_EQ(loop.Step(StepUnit::controlPeriod, 1), RefusalReason::stepWhileRunning);

    Pause();
    EXPECT_EQ(loop.Step(static_cast<StepUnit>(7), 1), RefusalReason::outOfRange);
}
