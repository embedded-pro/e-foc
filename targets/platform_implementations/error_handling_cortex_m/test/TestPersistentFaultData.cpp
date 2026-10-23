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

TEST_F(TestPersistentFaultData, fits_the_fault_region)
{
    static_assert(sizeof(application::PersistentFaultData) <= 1024);
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

TEST(TestFormatFaultData, worst_case_output_fits_max_formatted_size_without_truncation)
{
    application::PersistentFaultData data{};
    data.magic = application::PersistentFaultData::kMagicValid;
    data.r0 = data.r1 = data.r2 = data.r3 = data.r12 = data.lr = data.pc = data.psr = 0xFFFFFFFFu;
    data.cfsr = data.mmfar = data.bfar = 0xFFFFFFFFu;
    data.stackTraceCount = application::PersistentFaultData::kMaxStackTraceEntries + 1;
    data.stackTrace.fill(0xFFFFFFFFu);

    infra::BoundedString::WithStorage<application::PersistentFaultData::kMaxFormattedSize + 1> output;
    application::FormatFaultData(data, output);

    EXPECT_EQ(output.size(), application::PersistentFaultData::kMaxFormattedSize);
}
