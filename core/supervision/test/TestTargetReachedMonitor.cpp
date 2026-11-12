#include "core/supervision/TargetReachedMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

    class TargetReachedMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        TargetReachedMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    foc::MotionObservation observation{};
                    observation.targetError = foc::Radians{ error };
                    observation.positionError = foc::Radians{ referenceError };
                    return observation;
                }));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase*
                {
                    return inPositionMode ? &drive : nullptr;
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

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        bool inPositionMode{ true };
        float error{ 0.001f };
        float referenceError{ 0.0f };
        supervision::TargetReachedMonitor::Settings settings{ foc::Radians{ 0.01f }, std::chrono::milliseconds(20) };
        supervision::TargetReachedMonitor monitor;
    };
}

TEST_F(TargetReachedMonitorTest, an_error_within_the_window_for_the_time_reaches_the_target)
{
    Run(std::chrono::milliseconds(20));
    EXPECT_FALSE(monitor.IsReached());

    Run(std::chrono::milliseconds(1));
    EXPECT_TRUE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, an_error_short_of_the_setpoint_counts_as_well)
{
    error = -0.01f;

    Run(std::chrono::milliseconds(21));

    EXPECT_TRUE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, an_error_beyond_the_window_does_not_reach_the_target)
{
    error = 0.011f;

    Run(std::chrono::milliseconds(500));

    EXPECT_FALSE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, a_reference_that_still_runs_ahead_of_the_rotor_does_not_count_as_reached)
{
    error = 0.5f;
    referenceError = 0.001f;

    Run(std::chrono::milliseconds(500));

    EXPECT_FALSE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, an_error_that_leaves_the_window_starts_the_time_again)
{
    Run(std::chrono::milliseconds(15));
    error = 0.02f;
    Run(std::chrono::milliseconds(1));
    error = 0.001f;
    Run(std::chrono::milliseconds(20));
    EXPECT_FALSE(monitor.IsReached());

    Run(std::chrono::milliseconds(1));
    EXPECT_TRUE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, a_new_target_beyond_the_window_clears_the_flag_before_the_next_evaluation)
{
    Run(std::chrono::milliseconds(21));
    ASSERT_TRUE(monitor.IsReached());

    error = 1.5f;

    EXPECT_FALSE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, a_target_reached_again_waits_the_window_time_again)
{
    Run(std::chrono::milliseconds(21));
    error = 1.5f;
    Run(std::chrono::milliseconds(1));
    error = 0.001f;
    Run(std::chrono::milliseconds(20));
    EXPECT_FALSE(monitor.IsReached());

    Run(std::chrono::milliseconds(1));
    EXPECT_TRUE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, a_window_of_zero_never_reaches_the_target)
{
    settings.window = foc::Radians{ 0.0f };
    error = 0.0f;

    Run(std::chrono::milliseconds(500));

    EXPECT_FALSE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, a_drive_that_is_not_enabled_never_reaches_the_target)
{
    state = state_machine::Ready{};

    Run(std::chrono::milliseconds(500));

    EXPECT_FALSE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, leaving_enabled_clears_the_flag_and_starts_the_time_again)
{
    Run(std::chrono::milliseconds(21));
    state = state_machine::Ready{};
    EXPECT_FALSE(monitor.IsReached());

    Run(std::chrono::milliseconds(1));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(20));
    EXPECT_FALSE(monitor.IsReached());

    Run(std::chrono::milliseconds(1));
    EXPECT_TRUE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, another_control_mode_never_reaches_the_target)
{
    inPositionMode = false;

    Run(std::chrono::milliseconds(500));

    EXPECT_FALSE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, a_window_time_of_zero_reaches_the_target_at_the_first_evaluation)
{
    settings.time = infra::Duration{};

    Run(std::chrono::milliseconds(1));

    EXPECT_TRUE(monitor.IsReached());
}

TEST_F(TargetReachedMonitorTest, new_settings_are_in_force_from_the_next_evaluation)
{
    error = 0.02f;
    Run(std::chrono::milliseconds(100));
    EXPECT_FALSE(monitor.IsReached());

    settings = { foc::Radians{ 0.05f }, std::chrono::milliseconds(5) };
    Run(std::chrono::milliseconds(6));

    EXPECT_TRUE(monitor.IsReached());
}

TEST(TargetReachedSettings, come_from_the_position_window_and_its_time_in_the_configuration)
{
    services::ConfigData config{};
    config.limits.positionWindow = 0.02f;
    config.limits.positionWindowTime = 0.05f;

    const auto settings = supervision::TargetReachedSettingsFrom(config);

    EXPECT_NEAR(0.02f, settings.window.Value(), 1.0e-7f);
    EXPECT_EQ(std::chrono::milliseconds(50), std::chrono::duration_cast<std::chrono::milliseconds>(settings.time));
}

TEST(TargetReachedSettings, default_to_four_counts_of_the_default_encoder_for_twenty_milliseconds)
{
    const auto settings = supervision::TargetReachedSettingsFrom(services::ConfigData{});

    EXPECT_NEAR(2.0f * 3.14159265f * 4.0f / 4000.0f, settings.window.Value(), 1.0e-6f);
    EXPECT_EQ(std::chrono::milliseconds(20), std::chrono::duration_cast<std::chrono::milliseconds>(settings.time));
}
