#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/supervision/SupervisedInverter.hpp"
#include "core/supervision/SupervisedLowPriorityInterrupt.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    class SupervisedInverterTest
        : public testing::Test
    {
    public:
        foc::ProgressSignal progress;
        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        supervision::SupervisedInverter inverter{ inverterMock, progress };
    };

    class SupervisedLowPriorityInterruptTest
        : public testing::Test
    {
    public:
        foc::ProgressSignal progress;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        supervision::SupervisedLowPriorityInterrupt lowPriorityInterrupt{ lowPriorityInterruptMock, progress };
        infra::Function<void()> installedHandler;
        uint32_t handlerInvocations{ 0 };

        void RegisterCountingHandler()
        {
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).WillOnce(Invoke([this](const infra::Function<void()>& handler)
                {
                    installedHandler = handler;
                }));

            lowPriorityInterrupt.Register([this]()
                {
                    ++handlerInvocations;
                });
        }
    };
}

TEST_F(SupervisedInverterTest, writing_duty_cycles_signals_inner_loop_progress)
{
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverter.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{});

    EXPECT_TRUE(progress.TakeProgress());
}

TEST_F(SupervisedInverterTest, stopping_the_bridge_does_not_signal_progress)
{
    EXPECT_CALL(inverterMock, Stop());

    inverter.Stop();

    EXPECT_FALSE(progress.TakeProgress());
}

TEST_F(SupervisedInverterTest, queries_are_forwarded_to_the_wrapped_inverter)
{
    EXPECT_CALL(inverterMock, BaseFrequency()).WillOnce(Return(hal::Hertz{ 20000 }));
    EXPECT_CALL(inverterMock, MaxCurrentSupported()).WillOnce(Return(foc::Ampere{ 15.0f }));

    EXPECT_EQ(hal::Hertz{ 20000 }, inverter.BaseFrequency());
    EXPECT_NEAR(15.0f, inverter.MaxCurrentSupported().Value(), 1e-6f);
    EXPECT_FALSE(progress.TakeProgress());
}

TEST_F(SupervisedLowPriorityInterruptTest, no_progress_is_signalled_before_the_handler_runs)
{
    RegisterCountingHandler();

    EXPECT_FALSE(progress.TakeProgress());
}

TEST_F(SupervisedLowPriorityInterruptTest, running_the_outer_loop_handler_signals_progress)
{
    RegisterCountingHandler();

    installedHandler();

    EXPECT_TRUE(progress.TakeProgress());
    EXPECT_EQ(1u, handlerInvocations);
}

TEST_F(SupervisedLowPriorityInterruptTest, unregistering_stops_invoking_the_application_handler)
{
    RegisterCountingHandler();

    EXPECT_CALL(lowPriorityInterruptMock, Unregister());
    lowPriorityInterrupt.Unregister();
    installedHandler();

    EXPECT_EQ(0u, handlerInvocations);
}
