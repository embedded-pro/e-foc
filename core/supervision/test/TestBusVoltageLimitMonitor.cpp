#include "core/supervision/BusVoltageLimitMonitor.hpp"
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

    class BusVoltageLimitMonitorTest
        : public testing::Test
    {
    public:
        BusVoltageLimitMonitorTest()
        {
            EXPECT_CALL(drive, CurrentState()).WillRepeatedly(ReturnRef(state));

            monitor.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            monitor.Enable([this]()
                {
                    return settings;
                });
        }

        void Measure(float volts)
        {
            monitor.Evaluate(foc::Volts{ volts });
        }

        StrictMock<FocStateMachineBaseMock> drive;
        state_machine::State state{ state_machine::Enabled{} };
        supervision::BusVoltageLimitMonitor::Settings settings{ foc::Volts{ 20.0f }, foc::Volts{ 30.0f } };
        std::vector<state_machine::FaultCode> faults;
        supervision::BusVoltageLimitMonitor monitor{ [this](state_machine::FaultCode code)
            {
                faults.push_back(code);
            } };
    };
}

TEST_F(BusVoltageLimitMonitorTest, a_bus_within_the_limits_is_no_fault)
{
    Measure(24.0f);
    Measure(20.0f);
    Measure(30.0f);

    EXPECT_THAT(faults, IsEmpty());
}

TEST_F(BusVoltageLimitMonitorTest, a_bus_below_the_minimum_is_an_undervoltage)
{
    Measure(19.9f);

    EXPECT_THAT(faults, ElementsAre(state_machine::FaultCode::undervoltage));
}

TEST_F(BusVoltageLimitMonitorTest, a_bus_above_the_maximum_is_an_overvoltage)
{
    Measure(30.1f);

    EXPECT_THAT(faults, ElementsAre(state_machine::FaultCode::overvoltage));
}

TEST_F(BusVoltageLimitMonitorTest, a_limit_of_zero_is_off)
{
    settings = { foc::Volts{ 0.0f }, foc::Volts{ 0.0f } };

    Measure(0.0f);
    Measure(1000.0f);

    EXPECT_THAT(faults, IsEmpty());
}

TEST_F(BusVoltageLimitMonitorTest, each_limit_acts_on_its_own)
{
    settings = { foc::Volts{ 0.0f }, foc::Volts{ 30.0f } };
    Measure(1.0f);
    EXPECT_THAT(faults, IsEmpty());

    Measure(31.0f);
    EXPECT_THAT(faults, ElementsAre(state_machine::FaultCode::overvoltage));
}

TEST_F(BusVoltageLimitMonitorTest, the_settings_are_read_at_every_value)
{
    Measure(24.0f);
    settings.maximum = foc::Volts{ 23.0f };

    Measure(24.0f);

    EXPECT_THAT(faults, ElementsAre(state_machine::FaultCode::overvoltage));
}

TEST_F(BusVoltageLimitMonitorTest, the_fault_is_reported_once_until_the_drive_leaves_enabled)
{
    Measure(10.0f);
    Measure(10.0f);
    Measure(40.0f);
    EXPECT_THAT(faults, ElementsAre(state_machine::FaultCode::undervoltage));

    state = state_machine::Fault{};
    Measure(10.0f);
    state = state_machine::Enabled{};
    Measure(40.0f);

    EXPECT_THAT(faults, ElementsAre(state_machine::FaultCode::undervoltage, state_machine::FaultCode::overvoltage));
}

TEST_F(BusVoltageLimitMonitorTest, a_drive_that_is_not_enabled_is_not_watched)
{
    state = state_machine::Ready{};
    Measure(0.0f);
    state = state_machine::Idle{};
    Measure(40.0f);

    EXPECT_THAT(faults, IsEmpty());
}

TEST(BusVoltageLimitMonitor, a_value_before_it_is_attached_and_enabled_is_ignored)
{
    std::vector<state_machine::FaultCode> faults;
    supervision::BusVoltageLimitMonitor monitor{ [&faults](state_machine::FaultCode code)
        {
            faults.push_back(code);
        } };

    monitor.Evaluate(foc::Volts{ 0.0f });

    EXPECT_THAT(faults, IsEmpty());
}

TEST(BusVoltageLimitMonitor, the_settings_come_from_the_configuration_limits)
{
    services::ConfigData config{};
    config.limits.busVoltageMinimum = 18.0f;
    config.limits.busVoltageMaximum = 32.0f;

    const auto settings = supervision::BusVoltageLimitSettingsFrom(config);

    EXPECT_NEAR(settings.minimum.Value(), 18.0f, 1e-6f);
    EXPECT_NEAR(settings.maximum.Value(), 32.0f, 1e-6f);
}
