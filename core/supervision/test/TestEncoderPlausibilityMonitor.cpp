#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/supervision/EncoderPlausibilityMonitor.hpp"
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
            monitor.Enable({});
        }

        void PushWithoutMotion()
        {
            observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 5.0f }, foc::RadiansPerSecond{ 20.0f }, foc::Radians{ 0.0f } };
        }

        StrictMock<FocStateMachineBaseMock> drive;
        StrictMock<drivers::EncoderMock> encoder;
        state_machine::State state{ state_machine::Enabled{} };
        foc::MotionObservation observation{};
        float reading{ 1.0f };
        uint32_t losses{ 0 };
        supervision::EncoderPlausibilityMonitor monitor{ encoder, foc::Ampere{ maxCurrent }, [this]()
            {
                ++losses;
            } };
    };
}

TEST_F(EncoderPlausibilityMonitorTest, an_encoder_that_stays_put_while_current_pushes_for_motion_is_reported)
{
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(losses, 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, the_loss_is_reported_once_per_enable)
{
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(500));

    EXPECT_EQ(losses, 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_window_shorter_than_the_limit_is_not_reported)
{
    PushWithoutMotion();

    ForwardTime(std::chrono::milliseconds(80));

    EXPECT_EQ(losses, 0u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_moving_encoder_is_never_reported)
{
    PushWithoutMotion();

    for (int step = 0; step != 50; ++step)
    {
        reading += 0.05f;
        ForwardTime(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(losses, 0u);
}

TEST_F(EncoderPlausibilityMonitorTest, noise_below_the_excursion_limit_still_reads_as_still)
{
    PushWithoutMotion();

    for (int step = 0; step != 15; ++step)
    {
        reading = 1.0f + ((step % 2) != 0 ? 0.002f : -0.002f);
        ForwardTime(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(losses, 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, holding_still_against_a_load_is_not_a_demand_for_motion)
{
    observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 10.0f }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.01f } };

    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(losses, 0u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_disconnected_motor_carries_no_current_and_is_not_reported)
{
    observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 20.0f }, foc::Radians{ 0.0f } };

    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(losses, 0u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_position_error_the_loop_cannot_close_is_a_demand_for_motion)
{
    observation = { foc::RadiansPerSecond{ 0.0f }, foc::Ampere{ 5.0f }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 1.0f } };

    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(losses, 1u);
}

TEST_F(EncoderPlausibilityMonitorTest, a_drive_that_is_not_enabled_is_not_supervised)
{
    state = state_machine::Ready{};
    PushWithoutMotion();

    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(losses, 0u);
}

TEST_F(EncoderPlausibilityMonitorTest, re_enabling_rearms_the_report)
{
    PushWithoutMotion();
    ForwardTime(std::chrono::milliseconds(150));

    state = state_machine::Fault{ state_machine::FaultCode::encoderLoss };
    ForwardTime(std::chrono::milliseconds(20));
    state = state_machine::Enabled{};
    ForwardTime(std::chrono::milliseconds(150));

    EXPECT_EQ(losses, 2u);
}
