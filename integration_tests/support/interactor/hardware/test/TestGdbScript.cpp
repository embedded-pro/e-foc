#include "integration_tests/support/interactor/hardware/GdbScript.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace
{
    std::size_t PositionOf(const std::string& script, const std::string& line)
    {
        const auto position = script.find(line + "\n");
        EXPECT_NE(std::string::npos, position) << "missing line: " << line;
        return position;
    }

    class SlotImageTest
        : public ::testing::Test
    {
    protected:
        SlotImageTest()
        {
            std::filesystem::create_directories(directory);
        }

        ~SlotImageTest() override
        {
            std::filesystem::remove_all(directory);
        }

        void Touch(const std::filesystem::path& path)
        {
            std::ofstream{ path } << "x";
        }

        std::filesystem::path directory{ std::filesystem::temp_directory_path() / "e_foc_gdb_script_test" };
    };
}

TEST(GdbScriptTest, flash_script_loads_the_elf_and_restarts_the_target)
{
    const auto script = hil::FlashScript("probe:2331", std::nullopt, std::nullopt);

    EXPECT_LT(PositionOf(script, "target extended-remote probe:2331"), PositionOf(script, "load"));
    EXPECT_LT(PositionOf(script, "load"), PositionOf(script, "compare-sections"));
    EXPECT_LT(PositionOf(script, "compare-sections"), PositionOf(script, "monitor go"));
    EXPECT_EQ(std::string::npos, script.find("restore"));
}

TEST(GdbScriptTest, flash_script_writes_the_slot_image_over_the_whole_slot_after_the_load)
{
    const auto script = hil::FlashScript("probe:2331", std::nullopt, "/images/app.image.bin");

    const auto restore = PositionOf(script, "restore /images/app.image.bin binary (long)&__application_slot_start");
    EXPECT_LT(PositionOf(script, "load"), restore);
    EXPECT_LT(restore, PositionOf(script, "compare-sections"));
}

TEST(GdbScriptTest, flash_script_programs_the_bootloader_before_the_application)
{
    const auto script = hil::FlashScript("probe:2331", "/images/bootloader.elf", std::nullopt);

    const auto bootloader = PositionOf(script, "load /images/bootloader.elf");
    EXPECT_LT(PositionOf(script, "monitor halt"), bootloader);
    EXPECT_LT(bootloader, PositionOf(script, "load"));
}

TEST(GdbScriptTest, reset_script_resets_and_releases_the_target)
{
    const auto script = hil::ResetScript("probe:2331");

    EXPECT_LT(PositionOf(script, "target extended-remote probe:2331"), PositionOf(script, "monitor reset"));
    EXPECT_LT(PositionOf(script, "monitor reset"), PositionOf(script, "monitor go"));
    EXPECT_LT(PositionOf(script, "monitor go"), PositionOf(script, "disconnect"));
}

TEST_F(SlotImageTest, the_slot_image_is_the_image_beside_an_application_elf)
{
    Touch(directory / "app.elf");
    Touch(directory / "app.image.bin");

    EXPECT_EQ((directory / "app.image.bin").string(), hil::SlotImageOf((directory / "app.elf").string()));
}

TEST_F(SlotImageTest, an_elf_without_an_image_beside_it_has_no_slot_image)
{
    Touch(directory / "bringup.elf");

    EXPECT_EQ(std::nullopt, hil::SlotImageOf((directory / "bringup.elf").string()));
}
