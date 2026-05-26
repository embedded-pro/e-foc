#include "core/state_machine/ControlMode.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace state_machine;
    using namespace services;

    // -------- FromCanMode / ToCanMode roundtrip --------

    TEST(TestControlMode, FromCanMode_Torque_ReturnsTorque)
    {
        EXPECT_EQ(FromCanMode(FocMotorMode::torque), ControlMode::torque);
    }

    TEST(TestControlMode, FromCanMode_Speed_ReturnsSpeed)
    {
        EXPECT_EQ(FromCanMode(FocMotorMode::speed), ControlMode::speed);
    }

    TEST(TestControlMode, FromCanMode_Position_ReturnsPosition)
    {
        EXPECT_EQ(FromCanMode(FocMotorMode::position), ControlMode::position);
    }

    TEST(TestControlMode, ToCanMode_Torque_ReturnsTorque)
    {
        EXPECT_EQ(ToCanMode(ControlMode::torque), FocMotorMode::torque);
    }

    TEST(TestControlMode, ToCanMode_Speed_ReturnsSpeed)
    {
        EXPECT_EQ(ToCanMode(ControlMode::speed), FocMotorMode::speed);
    }

    TEST(TestControlMode, ToCanMode_Position_ReturnsPosition)
    {
        EXPECT_EQ(ToCanMode(ControlMode::position), FocMotorMode::position);
    }

    TEST(TestControlMode, FromCanMode_ToCanMode_Roundtrip_AllModes)
    {
        for (auto mode : { FocMotorMode::torque, FocMotorMode::speed, FocMotorMode::position })
            EXPECT_EQ(ToCanMode(FromCanMode(mode)), mode);
    }

    // -------- ToRejectReason --------

    TEST(TestControlMode, ToRejectReason_Ok_ReturnsOk)
    {
        EXPECT_EQ(ToRejectReason(SelectResult::ok), FocRejectReason::ok);
    }

    TEST(TestControlMode, ToRejectReason_Busy_ReturnsBusy)
    {
        EXPECT_EQ(ToRejectReason(SelectResult::busy), FocRejectReason::busy);
    }

    TEST(TestControlMode, ToRejectReason_NvmFailed_ReturnsNvmFailed)
    {
        EXPECT_EQ(ToRejectReason(SelectResult::nvmFailed), FocRejectReason::nvmFailed);
    }

    TEST(TestControlMode, ToRejectReason_InvalidMode_ReturnsInvalidPayload)
    {
        EXPECT_EQ(ToRejectReason(SelectResult::invalidMode), FocRejectReason::invalidPayload);
    }

    // -------- ControlModeFromRaw --------

    TEST(TestControlMode, ControlModeFromRaw_ZeroReturnsTorque)
    {
        EXPECT_EQ(ControlModeFromRaw(0), ControlMode::torque);
    }

    TEST(TestControlMode, ControlModeFromRaw_OneReturnsSpeed)
    {
        EXPECT_EQ(ControlModeFromRaw(1), ControlMode::speed);
    }

    TEST(TestControlMode, ControlModeFromRaw_TwoReturnsPosition)
    {
        EXPECT_EQ(ControlModeFromRaw(2), ControlMode::position);
    }

    TEST(TestControlMode, ControlModeFromRaw_UnknownValueDefaultsTorque)
    {
        EXPECT_EQ(ControlModeFromRaw(99), ControlMode::torque);
    }
}
