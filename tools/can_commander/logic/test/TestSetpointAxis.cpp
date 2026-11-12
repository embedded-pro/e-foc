#include "tools/can_commander/logic/SetpointAxis.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;

    class TestSetpointAxis
        : public testing::Test
    {};
}

TEST_F(TestSetpointAxis, offers_the_whole_firmware_limit)
{
    EXPECT_EQ(100000, TorqueAxis().MaxCounts());
    EXPECT_EQ(100000, SpeedAxis().MaxCounts());
    EXPECT_EQ("Accepted by the firmware", TorqueAxis().RangeLabel());
    EXPECT_EQ("Accepted by the firmware", SpeedAxis().RangeLabel());
}

TEST_F(TestSetpointAxis, offers_one_turn_of_position_because_the_loop_is_single_turn)
{
    EXPECT_EQ(3141, PositionAxis().MaxCounts());
    EXPECT_NEAR(179.966, PositionAxis().DisplayOf(PositionAxis().MaxCounts()), 1e-3);
    EXPECT_EQ("One turn from the alignment zero", PositionAxis().RangeLabel());
}

TEST_F(TestSetpointAxis, shows_speed_in_rpm_and_position_in_degrees)
{
    EXPECT_EQ(" rpm", SpeedAxis().Suffix());
    EXPECT_EQ("\xC2\xB0", PositionAxis().Suffix());
    EXPECT_NEAR(95.493, SpeedAxis().DisplayOfSi(10.0), 1e-3);
    EXPECT_NEAR(90.0, PositionAxis().DisplayOfSi(1.5707963), 1e-3);
}

TEST_F(TestSetpointAxis, steps_a_milliampere_a_hundredth_of_a_radian_per_second_and_a_milliradian)
{
    EXPECT_NEAR(0.001, TorqueAxis().DisplayStep(), 1e-12);
    EXPECT_EQ(3, TorqueAxis().Decimals());
    EXPECT_NEAR(0.01, SpeedAxis().DisplayStep() / SpeedAxis().DisplayOfSi(1.0), 1e-9);
    EXPECT_NEAR(0.001, PositionAxis().DisplayStep() / PositionAxis().DisplayOfSi(1.0), 1e-9);
}

TEST_F(TestSetpointAxis, snaps_a_typed_value_to_the_nearest_count)
{
    EXPECT_EQ(-2460, TorqueAxis().CountsOf(-2.46));
    EXPECT_EQ(1571, PositionAxis().CountsOf(90.0));
}

TEST_F(TestSetpointAxis, clamps_to_the_limit)
{
    EXPECT_EQ(100000, SpeedAxis().CountsOf(20000.0));
    EXPECT_EQ(-3141, PositionAxis().CountsOf(-270.0));
}

TEST_F(TestSetpointAxis, a_count_sends_its_exact_si_value)
{
    EXPECT_NEAR(3.25f, SpeedAxis().SiOf(325), 1e-6f);
    EXPECT_NEAR(-0.123f, TorqueAxis().SiOf(-123), 1e-6f);
    EXPECT_NEAR(1.571f, PositionAxis().SiOf(1571), 1e-6f);
}

TEST_F(TestSetpointAxis, picks_the_axis_of_each_control_mode)
{
    EXPECT_EQ(&TorqueAxis(), &AxisFor(can::FocMotorMode::torque));
    EXPECT_EQ(&SpeedAxis(), &AxisFor(can::FocMotorMode::speed));
    EXPECT_EQ(&PositionAxis(), &AxisFor(can::FocMotorMode::position));
}
