#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/supervision/EncoderPlausibilityMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <algorithm>
#include <cmath>
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

    class EncoderPlausibilityMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr float maxCurrent{ 20.0f };

        EncoderPlausibilityMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    return observation;
                }));
            EXPECT_CALL(encoder, Read()).WillRepeatedly(Invoke([this]()
                {
                    return foc::Radians{ reading };
                }));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            monitor.Enable([this]()
                {
                    return config;
                });
        }

        void PushWithoutMotion()
        {
            observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 5.0f }, foc::RadiansPerSecond{ 20.0f }, foc::Radians{ 0.0f } };
        }

        void PushTowards(float positionError)
        {
            observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ std::copysign(5.0f, positionError) }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ positionError } };
        }

        void Travel(float step, int periods)
        {
            for (int period = 0; period != periods; ++period)
            {
                reading += step;
                ForwardTime(std::chrono::milliseconds(10));
            }
        }

        std::size_t Count(state_machine::FaultCode code) const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, code));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        StrictMock<drivers::EncoderMock> encoder;
        state_machine::State state{ state_machine::Enabled{} };
        foc::MotionObservation observation{};
        float reading{ 1.0f };
        supervision::EncoderPlausibilityMonitor::Config config{ supervision::ConfigFrom(services::MakeDefaultConfigData()) };
        std::vector<state_machine::FaultCode> faults;
        supervision::EncoderPlausibilityMonitor monitor{ encoder, foc::Ampere{ maxCurrent }, [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(EncoderPlausibilityMonitorTest, an_encoder_that_stays_put_while_current_pushes_for_motion_is_reported)
{
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, the_loss_is_reported_once_per_enable)
{
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(500));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_window_shorter_than_the_limit_is_not_reported)
{
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(80));

    EXPECT_TRUE(faults.empty());
}

TEST_F(EncoderPlausibilityMonitorTest, a_moving_encoder_is_never_reported)
{
    PushWithoutMotion();

    for (int step = 0; step != 50; ++step)
    {
        reading += 0.05f;
        ForwardTime(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(faults.empty());
}

TEST_F(EncoderPlausibilityMonitorTest, noise_below_the_excursion_limit_still_reads_as_still)
{
    PushWithoutMotion();

    for (int step = 0; step != 15; ++step)
    {
        reading = 1.0f + ((step % 2) != 0 ? 0.002f : -0.002f);
        ForwardTime(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, holding_still_against_a_load_is_not_a_demand_for_motion)
{
    observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 10.0f }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.01f } };

    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(faults.empty());
}

TEST_F(EncoderPlausibilityMonitorTest, a_disconnected_motor_carries_no_current_and_is_not_reported)
{
    observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 20.0f }, foc::Radians{ 0.0f } };

    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(faults.empty());
}

TEST_F(EncoderPlausibilityMonitorTest, a_position_error_the_loop_cannot_close_is_a_demand_for_motion)
{
    observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 5.0f }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 1.0f } };

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_drive_that_is_not_enabled_is_not_supervised)
{
    state = state_machine::Ready{};
    PushWithoutMotion();

    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(faults.empty());
}

TEST_F(EncoderPlausibilityMonitorTest, re_enabling_rearms_the_report)
{
    PushWithoutMotion();
    ForwardTime(std::chrono::milliseconds(150));

    state = state_machine::Fault{ state_machine::FaultCode::encoderLoss };
    ForwardTime(std::chrono::milliseconds(20));
    state = state_machine::Enabled{};
    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 2u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_rotor_pushed_against_the_drive_and_then_held_is_reported_as_a_stall)
{
    PushTowards(-1.0f);
    Travel(0.05f, 5);

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::stall), 1u);
    EXPECT_EQ(faults.size(), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_rotor_twisted_against_the_demanded_speed_and_then_held_is_reported_as_a_stall)
{
    PushWithoutMotion();
    Travel(-0.05f, 5);

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::stall), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_reading_that_froze_after_moving_with_the_push_is_an_encoder_loss)
{
    PushTowards(1.0f);
    Travel(0.05f, 5);

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
    EXPECT_EQ(faults.size(), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, braking_towards_the_setpoint_is_not_being_overpowered)
{
    observation = { foc::RadiansPerSecond{ 5.0f }, foc::Ampere{ -5.0f }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 1.0f } };
    Travel(0.05f, 5);

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, travel_while_the_drive_does_not_push_clears_the_evidence_of_being_overpowered)
{
    PushTowards(-1.0f);
    Travel(0.05f, 3);
    observation = {};
    Travel(-0.05f, 3);

    PushTowards(-1.0f);
    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_push_that_crosses_the_thresholds_within_one_period_still_counts_as_overpowered)
{
    PushTowards(-0.1f);
    ForwardTime(std::chrono::milliseconds(10));

    PushTowards(-0.25f);
    Travel(0.15f, 1);
    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::stall), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, leaving_enabled_forgets_that_the_rotor_was_overpowered)
{
    PushTowards(-1.0f);
    Travel(0.05f, 3);

    state = state_machine::Ready{};
    ForwardTime(std::chrono::milliseconds(20));
    state = state_machine::Enabled{};
    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, with_stall_reporting_off_a_held_rotor_is_not_reported)
{
    config.stallDetection = false;
    PushTowards(-1.0f);
    Travel(0.05f, 5);

    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(faults.empty());
}

TEST_F(EncoderPlausibilityMonitorTest, with_stall_reporting_off_an_encoder_loss_is_still_reported)
{
    config.stallDetection = false;
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, with_stall_reporting_off_a_released_rotor_whose_reading_then_freezes_is_an_encoder_loss)
{
    config.stallDetection = false;
    PushTowards(-1.0f);
    Travel(0.05f, 5);
    ForwardTime(std::chrono::milliseconds(300));

    Travel(-0.05f, 2);
    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
    EXPECT_EQ(faults.size(), 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_new_window_takes_effect_at_the_next_evaluation)
{
    PushWithoutMotion();
    ForwardTime(std::chrono::milliseconds(50));

    config.window = std::chrono::milliseconds(500);
    ForwardTime(std::chrono::milliseconds(200));
    EXPECT_TRUE(faults.empty());

    ForwardTime(std::chrono::milliseconds(300));
    EXPECT_EQ(Count(state_machine::FaultCode::encoderLoss), 1u);
}

TEST(EncoderPlausibilityConfig, the_default_record_holds_the_documented_defaults)
{
    const auto config = supervision::ConfigFrom(services::MakeDefaultConfigData());

    EXPECT_EQ(config.window, std::chrono::milliseconds(100));
    EXPECT_NEAR(config.minimumCurrentFraction, 0.05f, 1e-6f);
    EXPECT_NEAR(config.minimumDemandedSpeed.Value(), 2.0f, 1e-6f);
    EXPECT_NEAR(config.minimumPositionError.Value(), 0.2f, 1e-6f);
    EXPECT_NEAR(config.maximumStillExcursion.Value(), 0.01f, 1e-6f);
    EXPECT_TRUE(config.stallDetection);
}

TEST(EncoderPlausibilityConfig, stored_settings_are_taken_as_written)
{
    auto data = services::MakeDefaultConfigData();
    data.supervision = { 2.0f, 0.2f, 5.0f, 0.5f, 0.02f };
    data.stallDetection = 0;

    const auto config = supervision::ConfigFrom(data);

    EXPECT_EQ(config.window, std::chrono::seconds(2));
    EXPECT_NEAR(config.minimumCurrentFraction, 0.2f, 1e-6f);
    EXPECT_NEAR(config.minimumDemandedSpeed.Value(), 5.0f, 1e-6f);
    EXPECT_NEAR(config.minimumPositionError.Value(), 0.5f, 1e-6f);
    EXPECT_NEAR(config.maximumStillExcursion.Value(), 0.02f, 1e-6f);
    EXPECT_FALSE(config.stallDetection);
}
