#include "core/supervision/PeakCurrent.hpp"
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

    class PeakCurrentTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Attach()
        {
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    foc::MotionObservation observation{};
                    observation.measuredTorqueCurrent = foc::Ampere{ current };
                    return observation;
                }));

            peak.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
        }

        void Drive(float value)
        {
            current = value;
            ForwardTime(supervision::PeakCurrent::samplePeriod);
        }

        StrictMock<FocStateMachineBaseMock> drive;
        float current{ 0.0f };
        supervision::PeakCurrent peak;
    };
}

TEST_F(PeakCurrentTest, without_a_drive_the_peak_is_zero)
{
    EXPECT_EQ(0.0f, peak.Peak().Value());
}

TEST_F(PeakCurrentTest, the_peak_is_the_sample_of_largest_magnitude_with_its_sign)
{
    Attach();

    Drive(1.0f);
    Drive(-3.0f);
    Drive(2.0f);

    EXPECT_NEAR(-3.0f, peak.Peak().Value(), 1e-6f);
}

TEST_F(PeakCurrentTest, a_sample_older_than_the_window_no_longer_counts)
{
    Attach();

    Drive(5.0f);
    for (std::size_t sample = 1; sample != supervision::PeakCurrent::window; ++sample)
        Drive(1.0f);
    EXPECT_NEAR(5.0f, peak.Peak().Value(), 1e-6f);

    Drive(1.0f);
    EXPECT_NEAR(1.0f, peak.Peak().Value(), 1e-6f);
}
