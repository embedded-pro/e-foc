#include "tools/can_commander/logic/FaultRecordAssembler.hpp"
#include "gtest/gtest.h"

namespace
{
    using tool::FaultRecordAssembler;

    class TestFaultRecordAssembler
        : public testing::Test
    {
    public:
        FaultRecordAssembler assembler;
    };
}

TEST_F(TestFaultRecordAssembler, conditions_complete_the_record_of_the_same_age)
{
    assembler.Record(3, 2, can::FocFaultCode::overCurrent, can::FocMotorMode::speed, 1234);

    const auto record = assembler.Conditions(3, 2, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f });

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(2, record->age);
    EXPECT_EQ(can::FocFaultCode::overCurrent, record->code);
    EXPECT_EQ(can::FocMotorMode::speed, record->mode);
    EXPECT_EQ(1234u, record->uptimeMs);
    EXPECT_EQ(5, record->powerUp);
    EXPECT_NEAR(24.0f, record->busVoltage.Value(), 1e-6f);
    EXPECT_NEAR(7.5f, record->peakCurrent.Value(), 1e-6f);
}

TEST_F(TestFaultRecordAssembler, a_record_completes_only_once)
{
    assembler.Record(3, 2, can::FocFaultCode::overCurrent, can::FocMotorMode::speed, 1234);
    ASSERT_TRUE(assembler.Conditions(3, 2, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f }).has_value());

    EXPECT_FALSE(assembler.Conditions(3, 2, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f }).has_value());
}

TEST_F(TestFaultRecordAssembler, conditions_of_another_age_or_node_complete_nothing)
{
    assembler.Record(3, 2, can::FocFaultCode::overCurrent, can::FocMotorMode::speed, 1234);

    EXPECT_FALSE(assembler.Conditions(3, 1, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f }).has_value());
    EXPECT_FALSE(assembler.Conditions(4, 2, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f }).has_value());
    EXPECT_TRUE(assembler.Conditions(3, 2, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f }).has_value());
}

TEST_F(TestFaultRecordAssembler, a_forgotten_node_has_no_record_to_complete)
{
    assembler.Record(3, 2, can::FocFaultCode::overCurrent, can::FocMotorMode::speed, 1234);
    assembler.Forget(3);

    EXPECT_FALSE(assembler.Conditions(3, 2, 5, foc::Volts{ 24.0f }, foc::Ampere{ 7.5f }).has_value());
}
