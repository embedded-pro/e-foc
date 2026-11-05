#include "core/supervision/FollowingErrorMonitor.hpp"
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
        MOCK_METHOD(foc::MotionObservation, ObserveMotion, (), (const, override));
    };

    class FollowingErrorMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        FollowingErrorMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    foc::MotionObservation observation{};
                    observation.positionError = foc::Radians{ error };
                    return observation;
                }));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            monitor.Enable([this]()
                {
                    return settings;
                });
        }

        void Run(std::chrono::milliseconds duration)
        {
            ForwardTime(duration);
        }

        std::size_t Reports() const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::followingError));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        float error{ 0.0f };
        supervision::FollowingErrorMonitor::Settings settings{ foc::Radians{ 0.5f }, std::chrono::milliseconds(100) };
        std::vector<state_machine::FaultCode> faults;
        supervision::FollowingErrorMonitor monitor{ [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(FollowingErrorMonitorTest, an_error_beyond_the_window_for_the_time_is_a_following_error)
{
    error = 0.6f;

    Run(std::chrono::milliseconds(100));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(1));
    EXPECT_EQ(1u, Reports());
}

TEST_F(FollowingErrorMonitorTest, an_error_behind_the_setpoint_counts_as_well)
{
    error = -0.6f;

    Run(std::chrono::milliseconds(101));

    EXPECT_EQ(1u, Reports());
}

TEST_F(FollowingErrorMonitorTest, an_error_within_the_window_is_not_a_following_error)
{
    error = 0.5f;

    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(FollowingErrorMonitorTest, an_error_that_comes_back_inside_the_window_starts_the_span_again)
{
    error = 0.6f;
    Run(std::chrono::milliseconds(90));
    error = 0.1f;
    Run(std::chrono::milliseconds(1));
    error = 0.6f;
    Run(std::chrono::milliseconds(100));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(2));
    EXPECT_EQ(1u, Reports());
}

TEST_F(FollowingErrorMonitorTest, a_window_of_zero_turns_the_check_off)
{
    settings.window = foc::Radians{ 0.0f };
    error = 3.0f;

    Run(std::chrono::milliseconds(1000));

    EXPECT_EQ(0u, Reports());
}

TEST_F(FollowingErrorMonitorTest, the_following_error_is_reported_once_until_the_drive_leaves_enabled)
{
    error = 1.0f;
    Run(std::chrono::milliseconds(500));
    EXPECT_EQ(1u, Reports());

    state = state_machine::Ready{};
    Run(std::chrono::milliseconds(1));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(102));

    EXPECT_EQ(2u, Reports());
}

TEST_F(FollowingErrorMonitorTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Ready{};
    error = 1.0f;

    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(FollowingErrorMonitorTest, new_settings_are_in_force_from_the_next_evaluation)
{
    error = 0.3f;
    Run(std::chrono::milliseconds(200));
    EXPECT_EQ(0u, Reports());

    settings = { foc::Radians{ 0.2f }, std::chrono::milliseconds(10) };
    Run(std::chrono::milliseconds(12));

    EXPECT_EQ(1u, Reports());
}
