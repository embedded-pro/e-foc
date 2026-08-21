#include "core/can/FocMotorMessages.hpp"
#include "core/state_machine/ControlMode.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace state_machine;

    TEST(TestControlMode, FromCanMode_Torque_ReturnsTorque)
    {
        EXPECT_EQ(FromCanMode(can::FocMotorMode::torque), ControlMode::torque);
    }

    TEST(TestControlMode, FromCanMode_Speed_ReturnsSpeed)
    {
        EXPECT_EQ(FromCanMode(can::FocMotorMode::speed), ControlMode::speed);
    }

    TEST(TestControlMode, FromCanMode_Position_ReturnsPosition)
    {
        EXPECT_EQ(FromCanMode(can::FocMotorMode::position), ControlMode::position);
    }

    TEST(TestControlMode, ToCanMode_Torque_ReturnsTorque)
    {
        EXPECT_EQ(ToCanMode(ControlMode::torque), can::FocMotorMode::torque);
    }

    TEST(TestControlMode, ToCanMode_Speed_ReturnsSpeed)
    {
        EXPECT_EQ(ToCanMode(ControlMode::speed), can::FocMotorMode::speed);
    }

    TEST(TestControlMode, ToCanMode_Position_ReturnsPosition)
    {
        EXPECT_EQ(ToCanMode(ControlMode::position), can::FocMotorMode::position);
    }

    TEST(TestControlMode, FromCanMode_ToCanMode_Roundtrip_AllModes)
    {
        for (auto mode : { can::FocMotorMode::torque, can::FocMotorMode::speed, can::FocMotorMode::position })
        {
            ASSERT_TRUE(FromCanMode(mode).has_value());
            EXPECT_EQ(ToCanMode(*FromCanMode(mode)), mode);
        }
    }

    TEST(TestControlMode, FromCanMode_UnrecognisedByte_ReturnsNullopt)
    {
        EXPECT_FALSE(FromCanMode(static_cast<can::FocMotorMode>(7)).has_value());
    }

    TEST(TestControlMode, ToAckStatus_Ok_ReturnsSuccess)
    {
        EXPECT_EQ(ToAckStatus(SelectResult::ok), services::CanAckStatus::success);
    }

    TEST(TestControlMode, ToAckStatus_Busy_ReturnsCategoryError)
    {
        EXPECT_EQ(ToAckStatus(SelectResult::busy), services::CanAckStatus::categoryError);
        EXPECT_EQ(ToCategoryError(SelectResult::busy), can::FocMotorCategoryError::busy);
    }

    TEST(TestControlMode, ToAckStatus_NvmFailed_ReturnsCategoryError)
    {
        EXPECT_EQ(ToAckStatus(SelectResult::nvmFailed), services::CanAckStatus::categoryError);
        EXPECT_EQ(ToCategoryError(SelectResult::nvmFailed), can::FocMotorCategoryError::persistenceFailed);
    }

    TEST(TestControlMode, ToAckStatus_InvalidMode_ReturnsInvalidPayload)
    {
        EXPECT_EQ(ToAckStatus(SelectResult::invalidMode), services::CanAckStatus::invalidPayload);
    }

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
