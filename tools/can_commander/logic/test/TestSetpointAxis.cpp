#include "tools/can_commander/logic/SetpointAxis.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;

    class TestSetpointAxis
        : public testing::Test
    {};
}

TEST_F(TestSetpointAxis, offers_the_firmware_limit_in_wire_counts)
{
    EXPECT_EQ(1000, TorqueAxis().MaxCounts());
    EXPECT_EQ(1000, SpeedAxis().MaxCounts());
    EXPECT_EQ("Accepted by the firmware", TorqueAxis().RangeLabel());
    EXPECT_EQ("Accepted by the firmware", SpeedAxis().RangeLabel());
}

TEST_F(TestSetpointAxis, offers_one_turn_of_position_because_the_loop_is_single_turn)
{
    EXPECT_EQ(314, PositionAxis().MaxCounts());
    EXPECT_NEAR(179.909, PositionAxis().DisplayOf(PositionAxis().MaxCounts()), 1e-3);
    EXPECT_EQ("One turn from the alignment zero", PositionAxis().RangeLabel());
}

TEST_F(TestSetpointAxis, shows_speed_in_rpm_and_position_in_degrees)
{
    EXPECT_EQ(" rpm", SpeedAxis().Suffix());
    EXPECT_EQ("\xC2\xB0", PositionAxis().Suffix());
    EXPECT_NEAR(9549.297, SpeedAxis().DisplayOf(SpeedAxis().MaxCounts()), 1e-3);
    EXPECT_NEAR(95.493, SpeedAxis().DisplayOfSi(10.0), 1e-3);
    EXPECT_NEAR(90.0, PositionAxis().DisplayOfSi(1.5707963), 1e-3);
}

TEST_F(TestSetpointAxis, shows_as_many_decimals_as_one_wire_count_needs)
{
    EXPECT_EQ(1, TorqueAxis().Decimals());
    EXPECT_EQ(0, SpeedAxis().Decimals());
    EXPECT_EQ(1, PositionAxis().Decimals());
}

TEST_F(TestSetpointAxis, snaps_a_typed_value_to_the_nearest_wire_count)
{
    EXPECT_EQ(10, SpeedAxis().CountsOf(100.0));
    EXPECT_EQ(157, PositionAxis().CountsOf(90.0));
    EXPECT_EQ(-25, TorqueAxis().CountsOf(-2.46));
}

TEST_F(TestSetpointAxis, clamps_to_the_limit)
{
    EXPECT_EQ(1000, SpeedAxis().CountsOf(20000.0));
    EXPECT_EQ(-314, PositionAxis().CountsOf(-270.0));
}

TEST_F(TestSetpointAxis, sends_the_exact_value_of_the_wire_count)
{
    EXPECT_NEAR(10.0f, SpeedAxis().SiOf(10), 1e-6f);
    EXPECT_NEAR(1.57f, PositionAxis().SiOf(157), 1e-6f);
    EXPECT_NEAR(-2.5f, TorqueAxis().SiOf(-25), 1e-6f);
}

TEST_F(TestSetpointAxis, picks_the_axis_of_each_control_mode)
{
    EXPECT_EQ(&TorqueAxis(), &AxisFor(can::FocMotorMode::torque));
    EXPECT_EQ(&SpeedAxis(), &AxisFor(can::FocMotorMode::speed));
    EXPECT_EQ(&PositionAxis(), &AxisFor(can::FocMotorMode::position));
}
