#include "core/supervision/PhaseLossMonitor.hpp"
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

    class PhaseLossMonitorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr float maxCurrent{ 10.0f };

        PhaseLossMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    return observation;
                }));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            monitor.Enable([this]()
                {
                    return currentLoopBandwidth;
                });
        }

        void Observe(float speed, float measured, float demanded)
        {
            observation = { foc::RadiansPerSecond{ speed }, foc::Ampere{ measured }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.0f }, foc::Ampere{ demanded } };
        }

        void Run(std::chrono::milliseconds duration)
        {
            ForwardTime(duration);
        }

        std::size_t Reports() const
        {
            return static_cast<std::size_t>(std::ranges::count(faults, state_machine::FaultCode::phaseMissing));
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        foc::MotionObservation observation{};
        foc::RadiansPerSecond currentLoopBandwidth{ 0.0f };
        std::vector<state_machine::FaultCode> faults;
        supervision::PhaseLossMonitor monitor{ foc::Ampere{ maxCurrent }, [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(PhaseLossMonitorTest, a_demand_that_draws_no_current_at_standstill_is_a_missing_phase_50_ms_after_it_is_first_seen)
{
    Observe(0.0f, 0.01f, 5.0f);

    Run(std::chrono::milliseconds(50));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(10));
    EXPECT_EQ(1u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_stopping_drive_is_watched_as_well)
{
    state = state_machine::Stopping{};
    Observe(0.0f, 0.01f, 5.0f);

    Run(std::chrono::milliseconds(50));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(10));
    EXPECT_EQ(1u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_shortfall_that_lasts_less_than_50_ms_is_not_reported_even_when_it_starts_just_before_an_evaluation)
{
    Run(std::chrono::milliseconds(9));
    Observe(0.0f, 0.0f, 5.0f);
    Run(std::chrono::milliseconds(45));
    Observe(0.0f, 5.0f, 5.0f);
    Run(std::chrono::milliseconds(100));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_healthy_drive_at_the_minimum_current_bandwidth_is_not_reported_while_its_current_rises)
{
    currentLoopBandwidth = foc::RadiansPerSecond{ 1.0f };
    Observe(0.0f, 0.0f, 5.0f);
    Run(std::chrono::milliseconds(220));
    Observe(0.0f, 1.1f, 5.0f);
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, an_open_circuit_at_the_minimum_current_bandwidth_is_reported_one_loop_time_constant_after_it_is_seen)
{
    currentLoopBandwidth = foc::RadiansPerSecond{ 1.0f };
    Observe(0.0f, 0.0f, 5.0f);

    Run(std::chrono::milliseconds(1000));
    EXPECT_EQ(0u, Reports());

    Run(std::chrono::milliseconds(10));
    EXPECT_EQ(1u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_fast_current_loop_keeps_the_50_ms_span)
{
    currentLoopBandwidth = foc::RadiansPerSecond{ 2000.0f };
    Observe(0.0f, 0.0f, 5.0f);

    Run(std::chrono::milliseconds(60));
    EXPECT_EQ(1u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_missing_phase_is_reported_once_per_enable)
{
    Observe(0.0f, 0.0f, -5.0f);
    Run(std::chrono::milliseconds(200));
    EXPECT_EQ(1u, Reports());

    state = state_machine::Ready{};
    Run(std::chrono::milliseconds(10));
    state = state_machine::Enabled{};
    Run(std::chrono::milliseconds(60));
    EXPECT_EQ(2u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_current_that_follows_its_demand_is_not_reported)
{
    Observe(0.0f, 4.9f, 5.0f);
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_demand_below_a_tenth_of_the_limit_does_not_arm_the_watch)
{
    Observe(0.0f, 0.0f, 0.99f);
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_current_just_above_a_fifth_of_the_demand_is_not_reported)
{
    Observe(0.0f, 1.05f, 5.0f);
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_fast_rotor_at_its_voltage_limit_is_not_reported)
{
    Observe(400.0f, 0.2f, 10.0f);
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_cable_pulled_while_running_is_reported_once_the_rotor_has_slowed)
{
    Observe(30.0f, 0.0f, 10.0f);
    Run(std::chrono::milliseconds(200));
    EXPECT_EQ(0u, Reports());

    Observe(-9.0f, 0.0f, 10.0f);
    Run(std::chrono::milliseconds(60));
    EXPECT_EQ(1u, Reports());
}

TEST_F(PhaseLossMonitorTest, an_evaluation_that_does_not_count_starts_the_span_again)
{
    Observe(0.0f, 0.0f, 5.0f);
    Run(std::chrono::milliseconds(40));
    Observe(0.0f, 5.0f, 5.0f);
    Run(std::chrono::milliseconds(10));
    Observe(0.0f, 0.0f, 5.0f);
    Run(std::chrono::milliseconds(40));

    EXPECT_EQ(0u, Reports());
}

TEST_F(PhaseLossMonitorTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Calibrating{};
    Observe(0.0f, 0.0f, 5.0f);
    Run(std::chrono::milliseconds(500));

    EXPECT_EQ(0u, Reports());
}
