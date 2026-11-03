#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <functional>
#include <string>

namespace
{
    using application::UpgradePackStatus;

    constexpr uint32_t applicationCapacity{ 2048 };
    constexpr const char* productName{ "e-foc test board" };
    constexpr uint32_t signatureLength{ 64 };
    constexpr uint32_t aesCounterSize{ 16 };

    template<std::size_t N>
    std::array<char, N> Text(const std::string& text)
    {
        std::array<char, N> result{};
        std::copy_n(text.begin(), std::min(text.size(), N), result.begin());
        return result;
    }

    struct PackDescription
    {
        std::string productName{ ::productName };
        std::string productVersion{ "2.3.4" };
        std::string target{ "app" };
        uint16_t headerVersion{ 1 };
        uint32_t numberOfImages{ 1 };
        uint32_t imageSize{ 300 };
        std::array<uint8_t, 3> magic{ 'U', 'P', 'H' };
        uint32_t trailing{ 0 };
    };

    struct Pack
    {
        services::PackHeader header;
        services::PackLimits limits;
    };

    Pack MakePack(const PackDescription& description = {})
    {
        Pack pack{};
        pack.header.prologue = application::UpgradePackHeaderPrologue{ UpgradePackStatus::readyToDeploy, description.magic, 0, 0, 2, signatureLength };
        pack.header.epilogue.headerVersion = description.headerVersion;
        pack.header.epilogue.headerLength = sizeof(pack.header.prologue) + signatureLength + sizeof(pack.header.epilogue);
        pack.header.epilogue.numberOfImages = description.numberOfImages;
        pack.header.epilogue.productName = Text<64>(description.productName);
        pack.header.epilogue.productVersion = Text<64>(description.productVersion);
        pack.header.image.lengthOfHeaderAndImage = sizeof(pack.header.image) + aesCounterSize + sizeof(application::ImageHeaderEpilogue) + description.imageSize;
        pack.header.image.targetName = Text<8>(description.target);
        pack.header.image.encryptionAndMacMethod = 1;

        const auto packSize = pack.header.epilogue.headerLength + pack.header.image.lengthOfHeaderAndImage + description.trailing;
        pack.limits = services::PackLimits{ productName, applicationCapacity, packSize };
        return pack;
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

    EXPECT_EQ(sizeof(application::UpgradePackHeaderPrologue) + signatureLength, services::EpilogueOffset(pack.header.prologue));
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
