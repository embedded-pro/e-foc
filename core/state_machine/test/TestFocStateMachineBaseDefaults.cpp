#include "core/state_machine/FocStateMachine.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using testing::Return;
    using testing::StrictMock;

    class MinimalStateMachine
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
    };

    class ObservingStateMachine
        : public MinimalStateMachine
    {
    public:
        MOCK_METHOD(foc::MotionObservation, ObserveMotion, (), (const, override));
    };
}

TEST(TestFocStateMachineBaseDefaults, an_asynchronous_enable_answers_with_the_synchronous_result)
{
    StrictMock<MinimalStateMachine> machine;
    EXPECT_CALL(machine, CmdEnable()).WillOnce(Return(state_machine::CommandResult::rejected));

    std::optional<state_machine::CommandResult> result;
    machine.CmdEnableAsync([&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
}

TEST(TestFocStateMachineBaseDefaults, a_mode_without_estimators_reports_no_online_estimates_and_no_motion)
{
    StrictMock<MinimalStateMachine> machine;

    EXPECT_FALSE(machine.CurrentOnlineEstimates().has_value());
    EXPECT_NEAR(0.0f, machine.MeasuredSpeed().Value(), 1e-6f);
}

TEST(TestFocStateMachineBaseDefaults, the_measured_speed_is_the_observed_one)
{
    StrictMock<ObservingStateMachine> machine;
    foc::MotionObservation observation{};
    observation.measuredSpeed = foc::RadiansPerSecond{ 12.5f };
    EXPECT_CALL(machine, ObserveMotion()).WillOnce(Return(observation));

    EXPECT_NEAR(12.5f, machine.MeasuredSpeed().Value(), 1e-6f);
}
