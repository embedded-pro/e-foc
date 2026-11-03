#include "infra/util/Crc.hpp"
#include "tools/can_commander/logic/FirmwarePack.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <string>
#include <vector>

namespace
{
    using namespace tool;

    constexpr uint32_t signatureLength{ 64 };
    constexpr uint32_t aesCounterSize{ 16 };

    template<class T>
    void Append(std::vector<uint8_t>& bytes, const T& value)
    {
        const auto* begin = reinterpret_cast<const uint8_t*>(&value);
        bytes.insert(bytes.end(), begin, begin + sizeof(T));
    }

    template<std::size_t N>
    std::array<char, N> Text(const std::string& text)
    {
        std::array<char, N> result{};
        std::copy_n(text.begin(), std::min(text.size(), N), result.begin());
        return result;
    }

    struct PackShape
    {
        std::string productName{ E_FOC_EK_TM4C1294XL_PRODUCT_NAME };
        std::string productVersion{ "1.7.0" };
        uint16_t headerVersion{ 1 };
        uint32_t imageSize{ 300 };
        std::array<uint8_t, 3> magic{ 'U', 'P', 'H' };
    };

    std::vector<uint8_t> MakePack(const PackShape& shape = {})
    {
        application::UpgradePackHeaderPrologue prologue{ application::UpgradePackStatus::readyToDeploy, shape.magic, 0, 0, 2, signatureLength };
        application::UpgradePackHeaderEpilogue epilogue{};
        epilogue.headerVersion = shape.headerVersion;
        epilogue.headerLength = sizeof(prologue) + signatureLength + sizeof(epilogue);
        epilogue.numberOfImages = 1;
        epilogue.productName = Text<64>(shape.productName);
        epilogue.productVersion = Text<64>(shape.productVersion);

        application::ImageHeaderPrologue image{};
        image.lengthOfHeaderAndImage = sizeof(image) + aesCounterSize + sizeof(application::ImageHeaderEpilogue) + shape.imageSize;
        image.targetName = Text<8>("app");
        image.encryptionAndMacMethod = 1;

        std::vector<uint8_t> pack;
        Append(pack, prologue);
        pack.insert(pack.end(), signatureLength, 0x5a);
        Append(pack, epilogue);
        Append(pack, image);
        pack.resize(pack.size() + image.lengthOfHeaderAndImage - sizeof(image), 0x33);
        return pack;
    }

    FirmwareBoard Board(uint8_t kind)
    {
        return *BoardOfKind(kind);
    }
}

TEST(FirmwarePack, every_board_kind_names_its_board_and_microcontroller)
{
    EXPECT_EQ("EK-TM4C1294XL", Board(1).name);
    EXPECT_EQ("TM4C1294NCPDT", Board(1).microcontroller);
    EXPECT_EQ("NUCLEO-G474RE", Board(2).name);
    EXPECT_EQ("NUCLEO-H563ZI", Board(3).name);
    EXPECT_EQ("EK-TM4C123GXL", Board(4).name);
    EXPECT_EQ("Emulated", Board(5).name);
    EXPECT_FALSE(BoardOfKind(0).has_value());
    EXPECT_FALSE(BoardOfKind(6).has_value());
}

TEST(FirmwarePack, only_boards_with_a_bootloader_can_be_updated)
{
    EXPECT_TRUE(CanBeUpdated(Board(1)));
    EXPECT_TRUE(CanBeUpdated(Board(5)));
    EXPECT_FALSE(CanBeUpdated(Board(2)));
    EXPECT_FALSE(CanBeUpdated(Board(3)));
    EXPECT_FALSE(CanBeUpdated(Board(4)));
}

TEST(FirmwarePack, a_pack_for_the_board_is_offered_with_its_version_and_crc)
{
    const auto bytes = MakePack();
    const auto inspection = InspectPack(bytes, Board(1));

    ASSERT_TRUE(inspection.pack.has_value());
    EXPECT_EQ(PackProblem::none, inspection.problem);
    EXPECT_EQ((services::FirmwareVersion{ 1, 7, 0 }), inspection.pack->version);
    infra::Crc32 crc;
    crc.Update(infra::MakeRange(bytes));
    EXPECT_EQ(crc.Result(), inspection.pack->crc32);
    EXPECT_EQ(bytes, inspection.pack->bytes);
}

TEST(FirmwarePack, a_pack_for_another_board_names_both_boards_and_microcontrollers)
{
    const auto inspection = InspectPack(MakePack(), Board(5));

    EXPECT_FALSE(inspection.pack.has_value());
    EXPECT_EQ(PackProblem::anotherTarget, inspection.problem);
    EXPECT_EQ("EK-TM4C1294XL", inspection.packBoard);
    EXPECT_EQ("TM4C1294NCPDT", inspection.packMicrocontroller);
    EXPECT_EQ("The pack is for EK-TM4C1294XL (TM4C1294NCPDT); this motor's board is Emulated (mps2-an386).", DescribePackProblem(inspection, Board(5)));
}

TEST(FirmwarePack, a_board_without_a_bootloader_is_offered_nothing)
{
    const auto inspection = InspectPack(MakePack(), Board(2));

    EXPECT_EQ(PackProblem::boardHasNoBootloader, inspection.problem);
    EXPECT_EQ("The NUCLEO-G474RE has no bootloader, so its firmware cannot be updated over CAN.", DescribePackProblem(inspection, Board(2)));
}

TEST(FirmwarePack, a_file_too_small_or_too_large_for_the_staging_slot_is_refused)
{
    EXPECT_EQ(PackProblem::tooSmall, InspectPack(std::vector<uint8_t>(100, 0), Board(1)).problem);
    EXPECT_EQ(PackProblem::tooLarge, InspectPack(std::vector<uint8_t>(stagingCapacity + 1, 0), Board(1)).problem);
}

TEST(FirmwarePack, a_file_without_the_pack_magic_is_not_a_pack)
{
    PackShape shape;
    shape.magic = { 'E', 'L', 'F' };

    EXPECT_EQ(PackProblem::notAPack, InspectPack(MakePack(shape), Board(1)).problem);
}

TEST(FirmwarePack, a_pack_with_another_header_version_is_refused)
{
    PackShape shape;
    shape.headerVersion = 2;

    EXPECT_EQ(PackProblem::unknownHeaderVersion, InspectPack(MakePack(shape), Board(1)).problem);
}

TEST(FirmwarePack, a_pack_without_a_release_version_or_with_an_image_too_large_is_not_an_application)
{
    PackShape unversioned;
    unversioned.productVersion = "development";
    PackShape oversized;
    oversized.imageSize = applicationCapacity + 4;

    EXPECT_EQ(PackProblem::notAnApplication, InspectPack(MakePack(unversioned), Board(1)).problem);
    EXPECT_EQ(PackProblem::tooLarge, InspectPack(MakePack(oversized), Board(1)).problem);
}

TEST(FirmwarePack, a_version_reads_as_major_minor_patch)
{
    EXPECT_EQ("1.7.0", VersionText({ 1, 7, 0 }));
}
