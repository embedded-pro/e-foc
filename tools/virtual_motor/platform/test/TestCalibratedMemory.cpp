#include "core/foc/model/drive/memory/PlantCalibration.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "tools/virtual_motor/platform/CalibratedMemory.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    std::vector<uint8_t> Contents(const std::string& path)
    {
        std::ifstream file{ path, std::ios::binary };
        return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
    }
}

TEST(TestCalibratedMemory, writes_the_memory_a_board_calibrated_on_the_plant_would_hold)
{
    const auto path = (std::filesystem::temp_directory_path() / "e-foc-calibrated-memory.nvm").string();
    sil::SilPlantConfig plant{};
    ASSERT_TRUE(sil::TryNamedPlant("anaheim", plant));

    EXPECT_TRUE(virtual_motor::WriteCalibratedMemory(path, plant));

    const auto expected = sil::BuildNvmImage(sil::CalibratedFor(plant));
    EXPECT_EQ(Contents(path), (std::vector<uint8_t>{ expected.begin(), expected.end() }));
    std::filesystem::remove(path);
}

TEST(TestCalibratedMemory, reports_a_memory_file_it_cannot_write)
{
    sil::SilPlantConfig plant{};
    ASSERT_TRUE(sil::TryNamedPlant("teknic", plant));

    EXPECT_FALSE(virtual_motor::WriteCalibratedMemory((std::filesystem::temp_directory_path() / "e-foc-missing-folder" / "m.nvm").string(), plant));
}
