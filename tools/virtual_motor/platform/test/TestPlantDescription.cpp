#include "infra/util/Crc.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "tools/virtual_motor/platform/PlantDescription.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    class TestPlantDescription
        : public testing::Test
    {
    public:
        ~TestPlantDescription() override
        {
            std::filesystem::remove(path);
        }

        void WriteRecord(const sil::SilPlantConfig& config, uint8_t version = sil::plantConfigLayoutVersion)
        {
            std::array<uint8_t, sil::plantConfigRecordSize> record{};
            const uint32_t magic = sil::plantConfigMagic;
            infra::Crc32 crc;
            crc.Update(infra::MakeConstByteRange(config));
            const uint32_t checksum = crc.Result();
            std::memcpy(record.data() + sil::plantConfigMagicOffset, &magic, sizeof(magic));
            record[sil::plantConfigVersionOffset] = version;
            std::memcpy(record.data() + sil::plantConfigCrcOffset, &checksum, sizeof(checksum));
            std::memcpy(record.data() + sil::plantConfigDataOffset, &config, sizeof(config));

            std::ofstream file{ path, std::ios::binary };
            file.write(reinterpret_cast<const char*>(record.data()), record.size());
        }

        std::string path{ (std::filesystem::temp_directory_path() / ("e-foc-plant-" + std::string{ ::testing::UnitTest::GetInstance()->current_test_info()->name() } + ".bin")).string() };
    };
}

TEST_F(TestPlantDescription, the_reference_motors_are_named_presets)
{
    const auto teknic = virtual_motor::PlantDescription("teknic");
    const auto anaheim = virtual_motor::PlantDescription("anaheim");

    ASSERT_TRUE(teknic.has_value());
    ASSERT_TRUE(anaheim.has_value());
    EXPECT_NE(teknic->statorResistanceOhm, anaheim->statorResistanceOhm);
}

TEST_F(TestPlantDescription, a_plant_file_is_read_as_the_emulated_target_reads_it)
{
    auto config = sil::NominalPlant();
    config.statorResistanceOhm = 1.25f;
    WriteRecord(config);

    const auto read = virtual_motor::PlantDescription(path);

    ASSERT_TRUE(read.has_value());
    EXPECT_FLOAT_EQ(read->statorResistanceOhm, 1.25f);
}

TEST_F(TestPlantDescription, a_file_of_another_layout_version_is_refused)
{
    WriteRecord(sil::NominalPlant(), sil::plantConfigLayoutVersion + 1);

    EXPECT_FALSE(virtual_motor::PlantDescription(path).has_value());
}

TEST_F(TestPlantDescription, an_unknown_name_that_is_no_file_is_refused)
{
    EXPECT_FALSE(virtual_motor::PlantDescription("no-such-plant").has_value());
}
