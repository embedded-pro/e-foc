#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include "core/services/firmware_upgrade/test_doubles/TestPack.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <functional>
#include <string>

namespace
{
    using PackDescription = services::test::TestPackDescription;
    using services::test::Text;

    constexpr uint32_t applicationCapacity{ 2048 };
    constexpr const char* productName{ services::test::testPackProductName };

    struct Pack
    {
        services::PackHeader header;
        services::PackLimits limits;
    };

    Pack MakePack(const PackDescription& description = {})
    {
        return Pack{ services::test::MakeTestPackHeader(description), services::PackLimits{ productName, applicationCapacity, services::test::TestPackSize(description) } };
    }

    std::optional<services::FirmwareVersion> Check(const PackDescription& description = {})
    {
        const auto pack = MakePack(description);
        return services::CheckPackHeader(pack.header, pack.limits);
    }

    std::optional<services::FirmwareVersion> Parse(const std::string& text)
    {
        return services::ParseVersion(Text<64>(text));
    }
}

TEST(PackHeaderCheckTest, a_pack_for_this_product_with_one_application_image_is_accepted_with_its_version)
{
    EXPECT_EQ((services::FirmwareVersion{ 2, 3, 4 }), Check());
}

TEST(PackHeaderCheckTest, an_image_that_fills_the_application_slot_exactly_is_accepted)
{
    PackDescription description;
    description.imageSize = applicationCapacity;

    EXPECT_TRUE(Check(description).has_value());
}

TEST(PackHeaderCheckTest, the_epilogue_follows_the_prologue_and_its_signature)
{
    const auto pack = MakePack();

    EXPECT_EQ(sizeof(application::UpgradePackHeaderPrologue) + services::test::testPackSignatureLength, services::EpilogueOffset(pack.header.prologue));
}

TEST(PackHeaderCheckTest, a_version_is_three_numbers_from_0_to_255)
{
    EXPECT_EQ((services::FirmwareVersion{ 0, 0, 0 }), Parse("0.0.0"));
    EXPECT_EQ((services::FirmwareVersion{ 255, 255, 255 }), Parse("255.255.255"));
    EXPECT_EQ((services::FirmwareVersion{ 1, 10, 100 }), Parse("1.10.100"));
}

TEST(PackHeaderCheckTest, anything_else_is_not_a_version)
{
    EXPECT_FALSE(Parse("256.0.0").has_value());
    EXPECT_FALSE(Parse("2.3").has_value());
    EXPECT_FALSE(Parse("2.3.4.5").has_value());
    EXPECT_FALSE(Parse("2..4").has_value());
    EXPECT_FALSE(Parse("2.3.4-rc1").has_value());
    EXPECT_FALSE(Parse("").has_value());
}

struct HeaderDefect
{
    const char* name;
    std::function<void(PackDescription&)> apply;
};

class PackHeaderDefectTest
    : public testing::TestWithParam<HeaderDefect>
{};

TEST_P(PackHeaderDefectTest, is_refused)
{
    PackDescription description;
    GetParam().apply(description);

    EXPECT_FALSE(Check(description).has_value());
}

INSTANTIATE_TEST_SUITE_P(Defects, PackHeaderDefectTest,
    testing::Values(
        HeaderDefect{ "another_product", [](PackDescription& d)
            {
                d.productName = "e-foc EK-TM4C1294XL TM4C1294NCPDT";
            } },
        HeaderDefect{ "a_longer_product_name", [](PackDescription& d)
            {
                d.productName = std::string{ productName } + " 2";
            } },
        HeaderDefect{ "bad_magic", [](PackDescription& d)
            {
                d.magic = { 'U', 'P', 'X' };
            } },
        HeaderDefect{ "header_version_2", [](PackDescription& d)
            {
                d.headerVersion = 2;
            } },
        HeaderDefect{ "two_images", [](PackDescription& d)
            {
                d.numberOfImages = 2;
            } },
        HeaderDefect{ "another_target", [](PackDescription& d)
            {
                d.target = "boot1st";
            } },
        HeaderDefect{ "an_image_larger_than_the_slot", [](PackDescription& d)
            {
                d.imageSize = applicationCapacity + 4;
            } },
        HeaderDefect{ "bytes_after_the_image", [](PackDescription& d)
            {
                d.trailing = 4;
            } },
        HeaderDefect{ "a_version_of_two_numbers", [](PackDescription& d)
            {
                d.productVersion = "2.3";
            } },
        HeaderDefect{ "a_version_with_letters", [](PackDescription& d)
            {
                d.productVersion = "2.3.4-rc1";
            } },
        HeaderDefect{ "an_empty_version", [](PackDescription& d)
            {
                d.productVersion = "";
            } },
        HeaderDefect{ "a_version_number_above_255", [](PackDescription& d)
            {
                d.productVersion = "2.256.4";
            } }),
    [](const testing::TestParamInfo<HeaderDefect>& info)
    {
        return std::string{ info.param.name };
    });
