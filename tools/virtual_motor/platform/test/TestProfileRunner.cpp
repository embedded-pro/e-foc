#include "tools/virtual_motor/platform/ProfileRunner.hpp"
#include "tools/virtual_motor/platform/test/InstrumentReportsMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace virtual_motor::port;

    MATCHER_P3(IsState, state, applied, elapsed, "")
    {
        return arg.state == state && arg.applied == applied && arg.elapsed == static_cast<uint32_t>(elapsed);
    }

    MATCHER_P(IsNear, expected, "")
    {
        return std::abs(arg.Value() - expected) < 1e-5f;
    }

    class DisturbanceInputsMock
        : public virtual_motor::DisturbanceInputs
    {
    public:
        MOCK_METHOD(void, SetLoadTorque, (foc::NewtonMeter torque), (override));
        MOCK_METHOD(void, SetExternalTorque, (foc::NewtonMeter torque), (override));
        MOCK_METHOD(void, SetCurrentNoise, (foc::Ampere sigma), (override));
        MOCK_METHOD(void, SetEncoderNoise, (foc::Radians sigma), (override));
    };

    ProfileStep Load(uint32_t at, float newtonMeter)
    {
        return ProfileStep{ at, MessageType::loadTorque, newtonMeter, {} };
    }

    Sample WithSpeed(float speed)
    {
        Sample sample{};
        sample[18] = speed;
        return sample;
    }

    class TestProfileRunner
        : public testing::Test
    {
    public:
        TestProfileRunner()
        {
            runner.Report(reports);
        }

        void Run(const ProfileRequest& profile)
        {
            EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::armed, 0, 0)));
            EXPECT_FALSE(runner.Run(profile).has_value());
        }

        void Observe(uint64_t from, uint64_t to, float speed = 0.0f)
        {
            for (auto period = from; period != to; ++period)
                runner.Observe(period, WithSpeed(speed), 0, false);
        }

        testing::StrictMock<DisturbanceInputsMock> inputs;
        virtual_motor::PlantDisturbances disturbances{ inputs, { foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, foc::Radians{ 0.0f } }, 1000 };
        testing::StrictMock<virtual_motor::InstrumentReportsMock> reports;
        virtual_motor::ProfileRunner runner{ disturbances };
    };
}

TEST_F(TestProfileRunner, an_immediate_profile_applies_each_change_in_the_period_its_time_names)
{
    Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, { Load(0, 0.1f), Load(5, 0.2f) } });

    testing::InSequence sequence;
    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.1f)));
    EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::running, 1, 0)));
    Observe(100, 105);

    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.2f)));
    EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::finished, 2, 5)));
    Observe(105, 120);

    EXPECT_FALSE(runner.Active());
}

TEST_F(TestProfileRunner, a_triggered_profile_waits_for_its_trigger)
{
    Run(ProfileRequest{ Trigger{ TriggerKind::rising, 18, 50.0f }, { Load(2, 0.3f) } });
    Observe(0, 10, 10.0f);

    EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::running, 0, 0)));
    Observe(10, 11, 60.0f);

    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.3f)));
    EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::finished, 1, 2)));
    Observe(11, 20, 60.0f);
}

TEST_F(TestProfileRunner, stopping_returns_the_plant_to_its_description)
{
    Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, { Load(0, 0.1f), Load(500, 0.2f) } });
    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.1f)));
    EXPECT_CALL(reports, ReportProfileState(testing::_));
    Observe(0, 3);

    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.0f)));
    EXPECT_CALL(inputs, SetCurrentNoise(testing::_));
    EXPECT_CALL(inputs, SetEncoderNoise(testing::_));
    EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::stopped, 1, 2)));
    runner.Stop();
    Observe(3, 1000);

    EXPECT_FALSE(runner.Active());
}

TEST_F(TestProfileRunner, abandoning_stops_and_leaves_what_was_applied_in_force)
{
    Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, { Load(0, 0.1f), Load(500, 0.2f) } });
    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.1f)));
    EXPECT_CALL(reports, ReportProfileState(testing::_));
    Observe(0, 3);

    EXPECT_CALL(reports, ReportProfileState(IsState(ProfileStateKind::stopped, 1, 2)));
    runner.Abandon();
    Observe(3, 1000);

    EXPECT_FLOAT_EQ(disturbances.InForce().loadTorque, 0.1f);
}

TEST_F(TestProfileRunner, refuses_a_profile_out_of_order_too_long_unreadable_or_with_a_change_out_of_range)
{
    EXPECT_EQ(runner.Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, { Load(5, 0.1f), Load(2, 0.2f) } }), RefusalReason::outOfRange);
    EXPECT_EQ(runner.Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, std::vector<ProfileStep>(1025, Load(0, 0.1f)) }), RefusalReason::outOfRange);
    EXPECT_EQ(runner.Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, { Load(0, 0.1f) }, false }), RefusalReason::outOfRange);
    EXPECT_EQ(runner.Run(ProfileRequest{ Trigger{ TriggerKind::immediately }, { ProfileStep{ 0, MessageType::currentNoise, -0.1f, {} } } }), RefusalReason::outOfRange);
    EXPECT_EQ(runner.Run(ProfileRequest{ Trigger{ TriggerKind::rising, 40, 0.0f }, {} }), RefusalReason::outOfRange);
    EXPECT_FALSE(runner.Active());
}
