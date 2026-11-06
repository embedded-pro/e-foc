#include "core/supervision/OverTemperature.hpp"
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

    class OverTemperatureTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        OverTemperatureTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                },
                [this]() -> std::optional<foc::Celsius>
                {
                    return temperature;
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

        std::size_t Trips() const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::overtemperature));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        std::optional<foc::Celsius> temperature{ foc::Celsius{ 25.0f } };
        supervision::OverTemperature::Settings settings{ foc::Celsius{ 120.0f }, foc::Celsius{ 15.0f } };
        std::vector<foc::Ampere> limits;
        std::vector<state_machine::FaultCode> faults;
        supervision::OverTemperature monitor{ foc::Ampere{ 10.0f },
            [this](foc::Ampere limit)
            {
                limits.push_back(limit);
            },
            [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(OverTemperatureTest, a_winding_below_the_margin_keeps_the_board_limit)
{
    temperature = foc::Celsius{ 105.0f };

    Run(std::chrono::milliseconds(200));

    EXPECT_NEAR(10.0f, monitor.Limit().Value(), 1e-5f);
    EXPECT_TRUE(limits.empty());
}

TEST_F(OverTemperatureTest, a_winding_within_the_margin_derates_the_limit_linearly)
{
    temperature = foc::Celsius{ 110.0f };

    Run(std::chrono::milliseconds(100));

    EXPECT_NEAR(10.0f * 10.0f / 15.0f, monitor.Limit().Value(), 1e-4f);
    ASSERT_FALSE(limits.empty());
    EXPECT_NEAR(10.0f * 10.0f / 15.0f, limits.back().Value(), 1e-4f);
}

TEST_F(OverTemperatureTest, a_winding_at_the_limit_derates_to_zero)
{
    temperature = foc::Celsius{ 120.0f };

    Run(std::chrono::milliseconds(100));

    EXPECT_NEAR(0.0f, monitor.Limit().Value(), 1e-6f);
}

TEST_F(OverTemperatureTest, a_cooling_winding_restores_the_board_limit)
{
    temperature = foc::Celsius{ 115.0f };
    Run(std::chrono::milliseconds(100));
    temperature = foc::Celsius{ 90.0f };
    Run(std::chrono::seconds(10));

    EXPECT_NEAR(10.0f, monitor.Limit().Value(), 1e-5f);
    ASSERT_FALSE(limits.empty());
    EXPECT_NEAR(10.0f, limits.back().Value(), 1e-5f);
}

TEST_F(OverTemperatureTest, a_jump_of_the_estimate_is_followed_with_a_one_second_time_constant)
{
    Run(std::chrono::milliseconds(100));
    temperature = foc::Celsius{ 170.0f };

    Run(std::chrono::milliseconds(100));
    EXPECT_NEAR(10.0f, monitor.Limit().Value(), 1e-5f);

    Run(std::chrono::seconds(2));
    EXPECT_NEAR(0.0f, monitor.Limit().Value(), 1e-6f);
}

TEST_F(OverTemperatureTest, the_filter_starts_again_from_the_estimate_once_the_temperature_was_unknown)
{
    temperature = std::nullopt;
    Run(std::chrono::milliseconds(100));
    temperature = foc::Celsius{ 110.0f };

    Run(std::chrono::milliseconds(100));

    EXPECT_NEAR(10.0f * 10.0f / 15.0f, monitor.Limit().Value(), 1e-4f);
}

TEST_F(OverTemperatureTest, an_unknown_temperature_keeps_the_board_limit_and_never_trips)
{
    temperature = std::nullopt;

    Run(std::chrono::seconds(10));

    EXPECT_NEAR(10.0f, monitor.Limit().Value(), 1e-5f);
    EXPECT_EQ(0u, Trips());
}

TEST_F(OverTemperatureTest, a_winding_at_the_limit_trips_once_five_seconds_after_the_enable)
{
    temperature = foc::Celsius{ 125.0f };

    Run(std::chrono::milliseconds(4900));
    EXPECT_EQ(0u, Trips());

    Run(std::chrono::milliseconds(200));
    EXPECT_EQ(1u, Trips());

    Run(std::chrono::seconds(5));
    EXPECT_EQ(1u, Trips());
}

TEST_F(OverTemperatureTest, a_winding_below_the_limit_does_not_trip)
{
    temperature = foc::Celsius{ 119.0f };

    Run(std::chrono::seconds(10));

    EXPECT_EQ(0u, Trips());
}

TEST_F(OverTemperatureTest, a_drive_that_is_not_enabled_does_not_trip)
{
    state = state_machine::Ready{};
    temperature = foc::Celsius{ 130.0f };

    Run(std::chrono::seconds(10));

    EXPECT_EQ(0u, Trips());
}

TEST_F(OverTemperatureTest, every_enable_waits_five_seconds_again_before_a_trip)
{
    temperature = foc::Celsius{ 130.0f };
    Run(std::chrono::milliseconds(5100));
    ASSERT_EQ(1u, Trips());

    state = state_machine::Ready{};
    Run(std::chrono::milliseconds(100));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(4800));
    EXPECT_EQ(1u, Trips());

    Run(std::chrono::milliseconds(300));
    EXPECT_EQ(2u, Trips());
}

TEST_F(OverTemperatureTest, new_settings_are_in_force_from_the_next_evaluation)
{
    temperature = foc::Celsius{ 90.0f };
    Run(std::chrono::milliseconds(100));
    EXPECT_NEAR(10.0f, monitor.Limit().Value(), 1e-5f);

    settings = { foc::Celsius{ 100.0f }, foc::Celsius{ 20.0f } };
    Run(std::chrono::milliseconds(100));

    EXPECT_NEAR(5.0f, monitor.Limit().Value(), 1e-4f);
}

TEST(OverTemperatureSettings, come_from_the_configuration)
{
    services::ConfigData config{};
    config.limits.temperatureLimit = 90.0f;
    config.limits.temperatureMargin = 10.0f;

    const auto settings = supervision::OverTemperatureSettingsFrom(config);

    EXPECT_NEAR(90.0f, settings.limit.Value(), 1e-6f);
    EXPECT_NEAR(10.0f, settings.margin.Value(), 1e-6f);
}

TEST(OverTemperatureSettings, default_to_120_degrees_with_a_15_kelvin_margin)
{
    const auto settings = supervision::OverTemperatureSettingsFrom(services::ConfigData{});

    EXPECT_NEAR(120.0f, settings.limit.Value(), 1e-6f);
    EXPECT_NEAR(15.0f, settings.margin.Value(), 1e-6f);
}
