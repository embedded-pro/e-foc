#include "core/services/firmware_upgrade/test_doubles/TestPack.hpp"
#include "generated/Boards.hpp"
#include "infra/util/Crc.hpp"
#include "tools/can_commander/logic/FirmwarePack.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace tool;

    struct PackShape
        : services::test::TestPackDescription
    {
        PackShape()
        {
            productName = e_foc::boards::ekTm4c1294xl.identification;
            productVersion = "1.7.0";
        }
    };

    std::vector<uint8_t> MakePack(const PackShape& shape = {})
    {
        return services::test::MakeTestPack(shape);
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
    EXPECT_EQ("emulated", Board(5).name);
    EXPECT_FALSE(BoardOfKind(0).has_value());
    EXPECT_FALSE(BoardOfKind(6).has_value());
}

TEST(FirmwarePack, every_board_carries_the_identification_its_firmware_is_built_with)
{
    EXPECT_EQ(std::string_view{ e_foc::boards::ekTm4c1294xl.identification }, Board(1).productName);
    EXPECT_EQ(std::string_view{ e_foc::boards::nucleoG474re.identification }, Board(2).productName);
    EXPECT_EQ(std::string_view{ e_foc::boards::nucleoH563zi.identification }, Board(3).productName);
    EXPECT_EQ(std::string_view{ e_foc::boards::ekTm4c123gxl.identification }, Board(4).productName);
    EXPECT_EQ(std::string_view{ e_foc::boards::emulated.identification }, Board(5).productName);
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
    EXPECT_EQ("The pack is for EK-TM4C1294XL (TM4C1294NCPDT); this motor's board is emulated (mps2-an386).", DescribePackProblem(inspection, Board(5)));
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
