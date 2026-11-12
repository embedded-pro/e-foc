#include "core/can/CanCommandTimeout.hpp"
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

    class CanCommandTimeoutTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        CanCommandTimeoutTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));

            watch.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            watch.Enable([this]()
                {
                    return timeout;
                });
        }

        void Run(std::chrono::milliseconds duration)
        {
            ForwardTime(duration);
        }

        std::size_t Reports() const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::commandTimeout));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        infra::Duration timeout{ std::chrono::milliseconds(500) };
        std::vector<state_machine::FaultCode> faults;
        can::CanCommandTimeout watch{ [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(CanCommandTimeoutTest, an_enabled_drive_without_commands_faults_once_the_timeout_runs_out)
{
    Run(std::chrono::milliseconds(500));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(1));
    EXPECT_EQ(1u, Reports());
}

TEST_F(CanCommandTimeoutTest, an_accepted_command_restarts_the_count)
{
    Run(std::chrono::milliseconds(400));
    watch.CommandAccepted();
    Run(std::chrono::milliseconds(499));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(2));
    EXPECT_EQ(1u, Reports());
}

TEST_F(CanCommandTimeoutTest, a_command_before_the_drive_enables_does_not_shorten_the_count)
{
    state = state_machine::Ready{};
    watch.CommandAccepted();
    Run(std::chrono::milliseconds(2000));

    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(500));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(2));
    EXPECT_EQ(1u, Reports());
}

TEST_F(CanCommandTimeoutTest, a_timeout_of_zero_turns_the_watch_off)
{
    timeout = infra::Duration::zero();

    Run(std::chrono::seconds(5));

    EXPECT_EQ(0u, Reports());
}

TEST_F(CanCommandTimeoutTest, a_timeout_written_while_the_drive_runs_counts_from_the_write)
{
    timeout = infra::Duration::zero();
    Run(std::chrono::seconds(5));

    timeout = std::chrono::milliseconds(100);
    Run(std::chrono::milliseconds(99));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(2));
    EXPECT_EQ(1u, Reports());
}

TEST_F(CanCommandTimeoutTest, the_timeout_is_reported_once_until_the_drive_leaves_enabled)
{
    Run(std::chrono::seconds(2));
    EXPECT_EQ(1u, Reports());

    state = state_machine::Fault{};
    Run(std::chrono::milliseconds(1));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(502));

    EXPECT_EQ(2u, Reports());
}

TEST_F(CanCommandTimeoutTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Ready{};

    Run(std::chrono::seconds(5));

    EXPECT_EQ(0u, Reports());
}
