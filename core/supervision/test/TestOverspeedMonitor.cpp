#include "core/supervision/OverspeedMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace
{
    using namespace testing;

    class FocStateMachineBaseMock
        : public state_machine::FocStateMachineBase
    {
    public:
        MOCK_METHOD(const state_machine::State&, CurrentState, (), (const, override));
        MOCK_METHOD(state_machine::FaultCode, LastFaultCode, (), (const, override));
        MOCK_METHOD(bool, HasPendingAsyncWork, (), (const, override));
        MOCK_METHOD(bool, HasPartialCalibration, (), (const, override));
        MOCK_METHOD(void, CmdCalibrate, (const infra::Function<void(state_machine::CommandResult)>& onDone), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdEnable, (), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdDisable, (), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdClearFault, (), (override));
        MOCK_METHOD(void, CmdClearCalibration, (const infra::Function<void(state_machine::CommandResult)>& onDone), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdEmergencyStop, (), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdQuickStop, (), (override));
        MOCK_METHOD(foc::MotionObservation, ObserveMotion, (), (const, override));
    };

    class OverspeedMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        OverspeedMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    return foc::MotionObservation{ foc::RadiansPerSecond{ speed } };
                }));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            monitor.Enable([this]()
                {
                    return foc::RadiansPerSecond{ limit };
                });
        }

        void Run(std::chrono::milliseconds duration)
        {
            ForwardTime(duration);
        }

        std::size_t Reports() const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::overspeed));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        float speed{ 0.0f };
        float limit{ 100.0f };
        std::vector<state_machine::FaultCode> faults;
        supervision::OverspeedMonitor monitor{ [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(OverspeedMonitorTest, a_speed_beyond_110_percent_of_the_limit_for_five_evaluations_is_an_overspeed)
{
    speed = 111.0f;

    Run(std::chrono::milliseconds(4));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(1));
    EXPECT_EQ(1u, Reports());
}

TEST_F(OverspeedMonitorTest, a_stopping_drive_is_watched_as_well)
{
    state = state_machine::Stopping{};
    speed = 111.0f;

    Run(std::chrono::milliseconds(4));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(1));
    EXPECT_EQ(1u, Reports());
}

TEST_F(OverspeedMonitorTest, a_speed_at_110_percent_of_the_limit_is_not_an_overspeed)
{
    speed = 110.0f;

    Run(std::chrono::milliseconds(100));

    EXPECT_EQ(0u, Reports());
}

TEST_F(OverspeedMonitorTest, a_reverse_speed_beyond_the_limit_is_an_overspeed)
{
    speed = -111.0f;

    Run(std::chrono::milliseconds(5));

    EXPECT_EQ(1u, Reports());
}

TEST_F(OverspeedMonitorTest, an_evaluation_within_the_limit_starts_the_count_again)
{
    speed = 111.0f;
    Run(std::chrono::milliseconds(4));
    speed = 105.0f;
    Run(std::chrono::milliseconds(1));
    speed = 111.0f;
    Run(std::chrono::milliseconds(4));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(1));
    EXPECT_EQ(1u, Reports());
}

TEST_F(OverspeedMonitorTest, the_overspeed_is_reported_once_until_the_drive_leaves_enabled)
{
    speed = 200.0f;
    Run(std::chrono::milliseconds(50));
    EXPECT_EQ(1u, Reports());

    state = state_machine::Ready{};
    Run(std::chrono::milliseconds(1));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(5));

    EXPECT_EQ(2u, Reports());
}

TEST_F(OverspeedMonitorTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Ready{};
    speed = 200.0f;

    Run(std::chrono::milliseconds(50));

    EXPECT_EQ(0u, Reports());
}

TEST_F(OverspeedMonitorTest, a_new_limit_is_in_force_from_the_next_evaluation)
{
    speed = 60.0f;
    Run(std::chrono::milliseconds(10));
    EXPECT_EQ(0u, Reports());

    limit = 50.0f;
    Run(std::chrono::milliseconds(5));

    EXPECT_EQ(1u, Reports());
}

TEST_F(OverspeedMonitorTest, an_unattached_monitor_raises_nothing)
{
    supervision::OverspeedMonitor unattached{ [this](state_machine::FaultCode code)
        {
            faults.push_back(code);
        } };
    unattached.Enable([this]()
        {
            return foc::RadiansPerSecond{ limit };
        });
    state = state_machine::Ready{};
    speed = 200.0f;

    Run(std::chrono::milliseconds(10));

    EXPECT_EQ(0u, Reports());
}
