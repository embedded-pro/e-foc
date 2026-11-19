#include "tools/virtual_motor/platform/DrivenShaft.hpp"
#include "gtest/gtest.h"
#include <initializer_list>

namespace
{
    using virtual_motor::ShaftDrive;

    class TestDrivenShaft
        : public testing::Test
    {
    public:
        void Record(std::initializer_list<float> torques)
        {
            for (auto torque : torques)
                shaft.Record(foc::NewtonMeter{ torque });
        }

        void Record(float torque, uint32_t periods)
        {
            for (uint32_t period = 0; period != periods; ++period)
                shaft.Record(foc::NewtonMeter{ torque });
        }

        static ShaftDrive Drive(float speed, uint64_t period, float torque)
        {
            return { foc::RadiansPerSecond{ speed }, period, foc::NewtonMeter{ torque }, foc::KilogramMeterSquared{ inertia } };
        }

        float SpeedNow() const
        {
            return shaft.Speed().value_or(foc::RadiansPerSecond{ -1.0e9f }).Value();
        }

        static constexpr float periodSeconds{ 0.1f };
        static constexpr float inertia{ 2.0f };

        virtual_motor::DrivenShaft shaft{ periodSeconds };
    };
}

TEST_F(TestDrivenShaft, a_shaft_never_driven_has_no_speed)
{
    Record({ 1.0f, 2.0f });

    EXPECT_FALSE(shaft.Speed().has_value());
}

TEST_F(TestDrivenShaft, adds_the_torque_deviation_of_the_periods_after_the_one_used)
{
    Record({ 9.0f, 9.0f, 1.0f, 2.0f, 4.0f });

    shaft.Drive(Drive(3.0f, 1, 0.5f));

    EXPECT_NEAR(SpeedNow(), 3.0f + (0.5f + 1.5f + 3.5f) * periodSeconds / inertia, 1e-5f);
}

TEST_F(TestDrivenShaft, keeps_adding_the_deviation_of_every_period_recorded_while_driven)
{
    Record({ 1.0f, 1.0f });
    shaft.Drive(Drive(-1.0f, 1, 0.5f));
    EXPECT_NEAR(SpeedNow(), -1.0f, 1e-6f);

    Record({ 2.5f, -0.5f });

    EXPECT_NEAR(SpeedNow(), -1.0f + (2.0f - 1.0f) * periodSeconds / inertia, 1e-5f);
}

TEST_F(TestDrivenShaft, a_used_period_not_yet_reached_gives_no_correction_until_after_it)
{
    Record({ 4.0f, 4.0f });
    shaft.Drive(Drive(1.0f, 3, 0.0f));
    EXPECT_NEAR(SpeedNow(), 1.0f, 1e-6f);

    Record({ 4.0f, 4.0f });
    EXPECT_NEAR(SpeedNow(), 1.0f, 1e-6f);

    Record({ 6.0f });
    EXPECT_NEAR(SpeedNow(), 1.0f + 6.0f * periodSeconds / inertia, 1e-5f);
}

TEST_F(TestDrivenShaft, periods_older_than_the_history_add_nothing)
{
    constexpr uint32_t olderPeriods{ 76 };
    Record(1000.0f, olderPeriods);
    Record(1.0f, virtual_motor::DrivenShaft::historyPeriods);

    shaft.Drive(Drive(0.0f, 0, 0.0f));

    EXPECT_NEAR(SpeedNow(), virtual_motor::DrivenShaft::historyPeriods * periodSeconds / inertia, 1e-3f);
}

TEST_F(TestDrivenShaft, a_release_drops_the_speed_and_a_new_drive_starts_from_its_own_backlog)
{
    Record({ 1.0f });
    shaft.Drive(Drive(2.0f, 0, 0.0f));
    Record({ 5.0f, 5.0f });

    shaft.Drive(std::nullopt);
    EXPECT_FALSE(shaft.Speed().has_value());
    Record({ 5.0f });

    shaft.Drive(Drive(2.0f, 3, 0.0f));
    EXPECT_NEAR(SpeedNow(), 2.0f, 1e-6f);
}
