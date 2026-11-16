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

        void WriteText(const std::string& lines)
        {
            std::ofstream file{ path };
            file << "e-foc plant description 1\n"
                 << lines;
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

TEST_F(TestPlantDescription, a_text_description_replaces_the_values_it_names_on_the_teknic_preset)
{
    WriteText("resistance 1.5\n"
              "inductance 0.001\n"
              "\n"
              "flux-linkage 0.009\n"
              "pole-pairs 7\n"
              "rotor-inertia 2e-5\n"
              "viscous-friction 3e-5\n"
              "coulomb-friction 0.002\n"
              "max-current 12\n"
              "supply-voltage 36\n");

    const auto read = virtual_motor::PlantDescription(path);

    ASSERT_TRUE(read.has_value());
    const auto teknic = *virtual_motor::PlantDescription("teknic");
    EXPECT_FLOAT_EQ(read->statorResistanceOhm, 1.5f);
    EXPECT_FLOAT_EQ(read->dAxisInductanceHenry, 0.001f);
    EXPECT_FLOAT_EQ(read->qAxisInductanceHenry, 0.001f);
    EXPECT_FLOAT_EQ(read->fluxLinkageWeber, 0.009f);
    EXPECT_EQ(read->polePairs, 7u);
    EXPECT_FLOAT_EQ(read->rotorInertiaKgM2, 2e-5f);
    EXPECT_FLOAT_EQ(read->viscousDampingNmSPerRad, 3e-5f);
    EXPECT_FLOAT_EQ(read->coulombFrictionNm, 0.002f);
    EXPECT_FLOAT_EQ(read->maxSupportedCurrentAmpere, 12.0f);
    EXPECT_FLOAT_EQ(read->powerSupplyVoltageVolts, 36.0f);
    EXPECT_EQ(read->encoderCountsPerRevolution, teknic.encoderCountsPerRevolution);
}

TEST_F(TestPlantDescription, a_text_description_starts_from_the_preset_it_names)
{
    WriteText("preset anaheim\n");

    const auto read = virtual_motor::PlantDescription(path);

    ASSERT_TRUE(read.has_value());
    EXPECT_FLOAT_EQ(read->statorResistanceOhm, virtual_motor::PlantDescription("anaheim")->statorResistanceOhm);
}

TEST_F(TestPlantDescription, load_inertia_adds_to_the_rotor_inertia_whichever_line_comes_first)
{
    WriteText("load-inertia 3e-5\n"
              "rotor-inertia 1e-5\n");

    const auto read = virtual_motor::PlantDescription(path);

    ASSERT_TRUE(read.has_value());
    EXPECT_FLOAT_EQ(read->rotorInertiaKgM2, 4e-5f);
}

TEST_F(TestPlantDescription, a_text_description_with_a_mistake_is_refused)
{
    for (const auto* mistake : { "speed 3\n", "resistance 1\nresistance 2\n", "resistance\n", "resistance one\n", "resistance 1.5x\n",
             "resistance 0\n", "resistance nan\n", "inductance inf\n", "load-inertia -1e-5\n", "coulomb-friction -0.1\n", "pole-pairs 0\n",
             "pole-pairs 256\n", "pole-pairs 2.5\n", "resistance 1\npreset anaheim\n", "preset bogus\n", "preset teknic\npreset anaheim\n",
             "resistance 1 2\n" })
    {
        WriteText(mistake);
        EXPECT_FALSE(virtual_motor::PlantDescription(path).has_value()) << mistake;
    }
}
