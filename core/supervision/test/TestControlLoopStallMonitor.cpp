#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/supervision/ControlLoopStallMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace
{
    using namespace testing;

    class ControlLoopStallMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        ControlLoopStallMonitorTest()
        {
            EXPECT_CALL(powerStage, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ 20000 }));
        }

        void StartStage()
        {
            EXPECT_CALL(powerStage, Start());
            stage.Start();
        }

        void StopStage()
        {
            EXPECT_CALL(powerStage, Stop());
            stage.Stop();
        }

        void CompleteFor(int milliseconds)
        {
            for (int i = 0; i != milliseconds * 4; ++i)
            {
                metrics.Record(100);
                ForwardTime(std::chrono::microseconds(250));
            }
        }

        StrictMock<drivers::ThreePhasePowerStageMock> powerStage;
        application::DecimatingThreePhaseInverter stage{ powerStage };
        application::ControlLoopMetrics metrics;
        application::PlatformDiagnostics diagnostics{ metrics };
        std::vector<state_machine::FaultCode> raised;
        supervision::ControlLoopStallMonitor monitor{ stage, diagnostics, [this](state_machine::FaultCode code)
            {
                raised.push_back(code);
            } };
    };
}

TEST_F(ControlLoopStallMonitorTest, a_stopped_stage_is_never_judged)
{
    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_TRUE(raised.empty());
}

TEST_F(ControlLoopStallMonitorTest, interrupts_that_keep_completing_raise_nothing)
{
    StartStage();
    CompleteFor(10);

    EXPECT_TRUE(raised.empty());
}

TEST_F(ControlLoopStallMonitorTest, a_start_with_no_completion_stops_the_stage_at_the_third_check)
{
    StartStage();
    ForwardTime(std::chrono::milliseconds(2));
    EXPECT_TRUE(raised.empty());

    EXPECT_CALL(powerStage, Stop());
    ForwardTime(std::chrono::milliseconds(1));

    EXPECT_THAT(raised, ElementsAre(state_machine::FaultCode::controlLoopStall));
    EXPECT_FALSE(stage.Started());
}

TEST_F(ControlLoopStallMonitorTest, interrupts_that_stop_completing_stop_the_stage_within_three_milliseconds)
{
    StartStage();
    CompleteFor(5);

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(raised.empty());

    EXPECT_CALL(powerStage, Stop());
    ForwardTime(std::chrono::milliseconds(2));

    EXPECT_THAT(raised, ElementsAre(state_machine::FaultCode::controlLoopStall));
}

TEST_F(ControlLoopStallMonitorTest, a_stall_is_raised_once)
{
    StartStage();
    CompleteFor(2);

    EXPECT_CALL(powerStage, Stop());
    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_EQ(1u, raised.size());
}

TEST_F(ControlLoopStallMonitorTest, stopping_the_stage_disarms_the_supervision)
{
    StartStage();
    CompleteFor(3);
    StopStage();

    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_TRUE(raised.empty());
}

TEST_F(ControlLoopStallMonitorTest, a_restart_between_checks_gets_its_own_grace)
{
    StartStage();
    CompleteFor(3);
    StopStage();
    StartStage();

    ForwardTime(std::chrono::milliseconds(1));
    CompleteFor(3);

    EXPECT_TRUE(raised.empty());
}

TEST_F(ControlLoopStallMonitorTest, clearing_the_loop_statistics_is_not_a_stall)
{
    StartStage();
    CompleteFor(2);
    diagnostics.Reset();
    CompleteFor(2);

    EXPECT_TRUE(raised.empty());
}

TEST_F(ControlLoopStallMonitorTest, a_check_that_runs_late_does_not_judge_the_short_gap_after_it)
{
    const auto start = infra::Now();
    StartStage();
    monitor.Check(start);
    metrics.Record(100);

    monitor.Check(start + std::chrono::microseconds(1950));
    monitor.Check(start + std::chrono::microseconds(2000));
    monitor.Check(start + std::chrono::microseconds(3000));

    EXPECT_TRUE(raised.empty());

    EXPECT_CALL(powerStage, Stop());
    monitor.Check(start + std::chrono::microseconds(3950));

    EXPECT_THAT(raised, ElementsAre(state_machine::FaultCode::controlLoopStall));
}
