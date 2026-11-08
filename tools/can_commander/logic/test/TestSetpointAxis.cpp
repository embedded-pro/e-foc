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

TEST_F(TestSetpointAxis, a_precise_axis_steps_finer_than_the_scaled_wire_count)
{
    EXPECT_NEAR(0.001, PreciseAxisFor(can::FocMotorMode::torque).DisplayStep(), 1e-12);
    EXPECT_EQ(3, PreciseAxisFor(can::FocMotorMode::torque).Decimals());
    EXPECT_NEAR(0.01, PreciseAxisFor(can::FocMotorMode::speed).DisplayStep() / PreciseAxisFor(can::FocMotorMode::speed).DisplayOfSi(1.0), 1e-9);
    EXPECT_NEAR(0.001, PreciseAxisFor(can::FocMotorMode::position).DisplayStep() / PreciseAxisFor(can::FocMotorMode::position).DisplayOfSi(1.0), 1e-9);
}

TEST_F(TestSetpointAxis, a_precise_axis_offers_the_whole_firmware_limit)
{
    EXPECT_EQ(100000, PreciseAxisFor(can::FocMotorMode::torque).MaxCounts());
    EXPECT_EQ(100000, PreciseAxisFor(can::FocMotorMode::speed).MaxCounts());
    EXPECT_EQ(3141, PreciseAxisFor(can::FocMotorMode::position).MaxCounts());
}

TEST_F(TestSetpointAxis, a_precise_count_sends_its_exact_si_value)
{
    EXPECT_NEAR(0.005f, PreciseAxisFor(can::FocMotorMode::speed).SiOf(1) * 0.5f, 1e-9f);
    EXPECT_NEAR(3.25f, PreciseAxisFor(can::FocMotorMode::speed).SiOf(325), 1e-6f);
    EXPECT_NEAR(-0.123f, PreciseAxisFor(can::FocMotorMode::torque).SiOf(-123), 1e-6f);
}

TEST_F(TestSetpointAxis, picks_the_precise_or_scaled_axis_by_the_contract)
{
    EXPECT_EQ(&SpeedAxis(), &AxisFor(can::FocMotorMode::speed, false));
    EXPECT_EQ(&PreciseAxisFor(can::FocMotorMode::speed), &AxisFor(can::FocMotorMode::speed, true));
}
