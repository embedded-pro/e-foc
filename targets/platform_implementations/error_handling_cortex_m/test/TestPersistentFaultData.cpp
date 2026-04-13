#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestPersistentFaultData : public ::testing::Test
    {
    protected:
        application::PersistentFaultData faultData{};
    };
}

TEST_F(TestPersistentFaultData, default_constructed_data_is_invalid)
{
    EXPECT_FALSE(faultData.IsValid());
}

TEST_F(TestPersistentFaultData, after_setting_magic_data_is_valid)
{
    faultData.magic = application::PersistentFaultData::kMagicValid;

    EXPECT_TRUE(faultData.IsValid());
}

TEST_F(TestPersistentFaultData, invalidate_clears_magic)
{
    faultData.magic = application::PersistentFaultData::kMagicValid;
    faultData.Invalidate();

    EXPECT_FALSE(faultData.IsValid());
}

TEST_F(TestPersistentFaultData, size_is_exactly_1024_bytes)
{
    static_assert(sizeof(application::PersistentFaultData) == 1024);
}

TEST(TestFormatFaultData, format_fault_data_includes_register_names)
{
    application::PersistentFaultData data{};
    data.magic = application::PersistentFaultData::kMagicValid;
    data.r0 = 0xDEAD0000u;
    data.pc = 0x08001234u;
    data.cfsr = 0x00000001u;
    data.stackTraceCount = 0;

    infra::BoundedString::WithStorage<1024> output;
    application::FormatFaultData(data, output);

    EXPECT_FALSE(output.empty());
    const std::string result(output.begin(), output.end());
    EXPECT_NE(result.find("R0"), std::string::npos);
    EXPECT_NE(result.find("PC"), std::string::npos);
    EXPECT_NE(result.find("CFSR"), std::string::npos);
}

TEST(TestFormatFaultData, format_fault_data_includes_stack_trace_when_present)
{
    application::PersistentFaultData data{};
    data.magic = application::PersistentFaultData::kMagicValid;
    data.stackTraceCount = 2;
    data.stackTrace[0] = 0x08001000u;
    data.stackTrace[1] = 0x08002000u;

    infra::BoundedString::WithStorage<1024> output;
    application::FormatFaultData(data, output);

    const std::string result(output.begin(), output.end());
    EXPECT_NE(result.find("Backtrace"), std::string::npos);
}
