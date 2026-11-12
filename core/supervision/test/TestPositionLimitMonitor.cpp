#include "core/supervision/PositionLimitMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <numbers>
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

    class PositionLimitMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        PositionLimitMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase*
                {
                    return inPositionMode ? &drive : nullptr;
                },
                [this]()
                {
                    return foc::Radians{ position };
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
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::positionLimit));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        bool inPositionMode{ true };
        float position{ 0.0f };
        supervision::PositionLimitMonitor::Settings settings{ foc::TravelLimits{ foc::Radians{ -1.0f }, foc::Radians{ 1.0f } }, foc::Radians{ 0.01f } };
        std::vector<state_machine::FaultCode> faults;
        supervision::PositionLimitMonitor monitor{ [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(PositionLimitMonitorTest, a_rotor_beyond_the_maximum_by_more_than_the_margin_faults_at_the_next_evaluation)
{
    position = 1.011f;

    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(1u, Reports());
}

TEST_F(PositionLimitMonitorTest, a_stopping_drive_is_watched_as_well)
{
    state = state_machine::Stopping{};
    position = 1.011f;

    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(1u, Reports());
}

TEST_F(PositionLimitMonitorTest, a_rotor_beyond_the_minimum_by_more_than_the_margin_faults_as_well)
{
    position = -1.011f;

    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(1u, Reports());
}

TEST_F(PositionLimitMonitorTest, a_rotor_within_the_margin_of_a_limit_does_not_fault)
{
    position = 1.009f;

    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PositionLimitMonitorTest, a_rotor_pushed_across_the_seam_reads_far_outside_and_faults)
{
    settings.limits = foc::TravelLimits{ foc::Radians{ -3.0f }, foc::Radians{ 3.0f } };
    position = -std::numbers::pi_v<float> + 0.05f;

    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(1u, Reports());
}

TEST_F(PositionLimitMonitorTest, the_fault_is_reported_once_until_the_drive_leaves_enabled)
{
    position = 2.0f;
    Run(std::chrono::milliseconds(100));
    EXPECT_EQ(1u, Reports());

    state = state_machine::Ready{};
    Run(std::chrono::milliseconds(1));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(2u, Reports());
}

TEST_F(PositionLimitMonitorTest, limits_that_are_off_are_not_watched)
{
    settings.limits = foc::TravelLimits{};
    position = 2.0f;

    Run(std::chrono::milliseconds(100));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PositionLimitMonitorTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Ready{};
    position = 2.0f;

    Run(std::chrono::milliseconds(100));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PositionLimitMonitorTest, another_control_mode_is_not_watched)
{
    inPositionMode = false;
    position = 2.0f;

    Run(std::chrono::milliseconds(100));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PositionLimitMonitorTest, new_limits_are_in_force_from_the_next_evaluation)
{
    position = 0.8f;
    Run(std::chrono::milliseconds(10));
    EXPECT_EQ(0u, Reports());

    settings.limits = foc::TravelLimits{ foc::Radians{ -0.5f }, foc::Radians{ 0.5f } };
    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(1u, Reports());
}

TEST(PositionLimitSettings, come_from_the_travel_limits_and_four_counts_of_the_encoder)
{
    services::ConfigData config{};
    config.limits.positionMinimum = -1.5f;
    config.limits.positionMaximum = 2.5f;
    config.encoderResolution = 8000;

    const auto settings = supervision::PositionLimitSettingsFrom(config);

    EXPECT_NEAR(-1.5f, settings.limits.minimum.Value(), 1e-7f);
    EXPECT_NEAR(2.5f, settings.limits.maximum.Value(), 1e-7f);
    EXPECT_NEAR(2.0f * std::numbers::pi_v<float> * 4.0f / 8000.0f, settings.margin.Value(), 1e-7f);
}
