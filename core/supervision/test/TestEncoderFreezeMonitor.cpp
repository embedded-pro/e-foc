#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/supervision/EncoderFreezeMonitor.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
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
        MOCK_METHOD(foc::MotionObservation, ObserveMotion, (), (const, override));
    };

    class EncoderFreezeMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr float step{ 2.0f * std::numbers::pi_v<float> / 4000.0f };

        EncoderFreezeMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));
            EXPECT_CALL(encoder, Read()).WillRepeatedly(Invoke([this]()
                {
                    return foc::Radians{ Quantised(rotor(Elapsed())) };
                }));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            monitor.Enable([this]()
                {
                    return supervision::EncoderFreezeMonitor::Limits{ foc::RadiansPerSecondSquared{ alpha }, foc::Radians{ step } };
                });
        }

        float Elapsed() const
        {
            return std::chrono::duration<float>(infra::Now() - start).count();
        }

        static float Quantised(float angle)
        {
            return std::round(angle / step) * step;
        }

        void Run(std::chrono::milliseconds duration)
        {
            ForwardTime(duration);
        }

        std::size_t Reports() const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::encoderLoss));
        }

        static std::function<float(float)> TurningUntil(float speed, float freezeAt)
        {
            return [speed, freezeAt](float t)
            {
                return speed * std::min(t, freezeAt);
            };
        }

        float alpha{ 10000.0f };
        StrictMock<FocStateMachineBaseMock> drive;
        StrictMock<drivers::EncoderMock> encoder;
        state_machine::State state{ state_machine::Enabled{} };
        infra::TimePoint start{ infra::Now() };
        std::function<float(float)> rotor{ [](float)
            {
                return 0.0f;
            } };
        std::vector<state_machine::FaultCode> faults;
        supervision::EncoderFreezeMonitor monitor{ encoder, [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(EncoderFreezeMonitorTest, a_reading_that_stops_dead_at_speed_is_an_encoder_loss_after_5_ms)
{
    rotor = TurningUntil(100.0f, 0.050f);

    Run(std::chrono::milliseconds(54));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(2));
    EXPECT_EQ(1u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_freeze_part_way_through_a_period_is_judged_on_the_full_period_before_it)
{
    rotor = TurningUntil(100.0f, 0.0503f);

    Run(std::chrono::milliseconds(60));

    EXPECT_EQ(1u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_freeze_is_reported_once_per_enable)
{
    rotor = TurningUntil(100.0f, 0.050f);
    Run(std::chrono::milliseconds(200));
    EXPECT_EQ(1u, Reports());

    state = state_machine::Ready{};
    Run(std::chrono::milliseconds(1));
    rotor = [](float t)
    {
        return 100.0f * std::min(t, 0.300f);
    };
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(200));
    EXPECT_EQ(2u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_rotor_held_still_is_not_reported)
{
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_rotor_that_stops_within_the_deceleration_bound_is_not_reported)
{
    constexpr float speed = 100.0f;
    const float braking = 0.8f * alpha;
    constexpr float brakeAt = 0.050f;
    const float stopAfter = speed / braking;
    rotor = [=](float t)
    {
        if (t < brakeAt)
            return speed * t;

        const auto braked = std::min(t - brakeAt, stopAfter);
        return speed * brakeAt + speed * braked - 0.5f * braking * braked * braked;
    };

    Run(std::chrono::milliseconds(300));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_stop_the_deceleration_bound_allows_within_one_period_is_not_reported)
{
    alpha = 1.0e6f;
    rotor = TurningUntil(100.0f, 0.050f);

    Run(std::chrono::milliseconds(300));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_hard_stop_that_rebounds_within_the_confirmation_is_not_reported)
{
    rotor = [](float t)
    {
        const auto atStop = 100.0f * std::min(t, 0.050f);
        if (t >= 0.052f && t < 0.0525f)
            return atStop - 10.0f * step;
        return atStop;
    };

    Run(std::chrono::milliseconds(51));
    rotor = [](float t)
    {
        return t < 0.053f ? 5.0f : 5.0f - 10.0f * step;
    };
    Run(std::chrono::milliseconds(300));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, quantisation_of_a_slow_rotor_is_not_reported)
{
    rotor = [](float t)
    {
        return 0.5f * t + (std::fmod(t, 0.010f) < 0.005f ? 0.0f : step);
    };

    Run(std::chrono::milliseconds(1000));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_freeze_below_the_minimum_speed_is_not_reported)
{
    rotor = TurningUntil(15.0f, 0.050f);

    Run(std::chrono::milliseconds(300));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Ready{};
    EXPECT_CALL(encoder, Read()).Times(0);
    rotor = TurningUntil(100.0f, 0.050f);

    Run(std::chrono::milliseconds(300));

    EXPECT_EQ(0u, Reports());
}

TEST_F(EncoderFreezeMonitorTest, a_reading_that_moves_again_before_the_confirmation_starts_the_watch_again)
{
    rotor = [](float t)
    {
        if (t < 0.050f)
            return 100.0f * t;
        if (t < 0.053f)
            return 5.0f;
        return 5.0f + 100.0f * (t - 0.053f);
    };

    Run(std::chrono::milliseconds(56));

    EXPECT_EQ(0u, Reports());
}

TEST(EncoderFreezeLimitsTest, a_calibrated_record_bounds_the_deceleration_by_twice_the_motor_torque_over_its_inertia)
{
    services::CalibrationData calibration;
    calibration.polePairs = 4;
    calibration.fluxLinkage = 0.0064f;
    calibration.inertia = 0.00000706f;

    const auto limits = supervision::LimitsFrom(calibration, 4000, foc::Ampere{ 3.0f });

    EXPECT_NEAR(2.0f * 1.5f * 4.0f * 0.0064f * 3.0f / 0.00000706f, limits.maximumDeceleration.Value(), 1.0f);
    EXPECT_NEAR(2.0f * std::numbers::pi_v<float> / 4000.0f, limits.encoderStep.Value(), 1e-7f);
}

TEST(EncoderFreezeLimitsTest, a_record_without_inertia_falls_back_to_a_light_motor)
{
    services::CalibrationData calibration;
    calibration.polePairs = 4;
    calibration.fluxLinkage = 0.0064f;

    EXPECT_NEAR(2.0f * 2.0e4f * 3.0f, supervision::LimitsFrom(calibration, 4000, foc::Ampere{ 3.0f }).maximumDeceleration.Value(), 1.0f);
}

TEST(EncoderFreezeLimitsTest, a_record_without_flux_linkage_falls_back_to_a_light_motor)
{
    services::CalibrationData calibration;
    calibration.polePairs = 4;
    calibration.inertia = 0.00000706f;

    EXPECT_NEAR(2.0f * 2.0e4f * 3.0f, supervision::LimitsFrom(calibration, 4000, foc::Ampere{ 3.0f }).maximumDeceleration.Value(), 1.0f);
}
