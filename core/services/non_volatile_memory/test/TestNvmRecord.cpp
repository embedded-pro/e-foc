#include "core/services/non_volatile_memory/NvmRecord.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    services::CalibrationData Calibration()
    {
        services::CalibrationData data{};
        data.rPhase = 0.36f;
        data.lD = 0.2f;
        data.polePairs = 4;
        data.currentNoiseMilliAmpere = 1000;
        data.stage = services::CalibrationStage::complete;
        data.coggingTorque[3] = -7;
        return data;
    }

    class NvmRecordTest
        : public testing::Test
    {
    public:
        std::array<uint8_t, services::CalibrationRecord::size> image{};

        void Serialize(const services::CalibrationData& data)
        {
            services::CalibrationRecord::Serialize(data, infra::MakeRange(image));
        }

        std::optional<services::CalibrationData> Deserialize() const
        {
            return services::CalibrationRecord::Deserialize(infra::MakeConstByteRange(image));
        }
    };
}

TEST_F(NvmRecordTest, the_calibration_record_is_its_header_and_one_hundred_data_bytes)
{
    EXPECT_EQ(109u, services::CalibrationRecord::size);
    EXPECT_EQ(177u, services::ConfigRecord::size);
    EXPECT_EQ(6, services::CalibrationLayoutVersion);
}

TEST_F(NvmRecordTest, the_header_holds_the_magic_and_the_crc_least_significant_byte_first)
{
    Serialize(Calibration());

    EXPECT_EQ((std::array<uint8_t, 5>{ 0x0D, 0xF0, 0xFE, 0xCA, services::CalibrationLayoutVersion }), (std::array<uint8_t, 5>{ image[0], image[1], image[2], image[3], image[4] }));
    const auto data = infra::DiscardHead(infra::MakeConstByteRange(image), services::CalibrationRecord::dataOffset);
    EXPECT_EQ(services::nvm::Crc32(data), services::nvm::Get<uint32_t>(infra::MakeConstByteRange(image), services::CalibrationRecord::crcOffset));
}

TEST_F(NvmRecordTest, a_record_reads_back_what_was_written)
{
    Serialize(Calibration());

    const auto read = Deserialize();

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(0.36f, read->rPhase);
    EXPECT_EQ(0.2f, read->lD);
    EXPECT_EQ(4, read->polePairs);
    EXPECT_EQ(1000, read->currentNoiseMilliAmpere);
    EXPECT_EQ(services::CalibrationStage::complete, read->stage);
    EXPECT_EQ(-7, read->coggingTorque[3]);
}

TEST_F(NvmRecordTest, a_wrong_magic_version_or_crc_reads_as_no_record)
{
    for (const auto offset : { std::size_t{ 0 }, services::CalibrationRecord::versionOffset, services::CalibrationRecord::crcOffset, services::CalibrationRecord::dataOffset })
    {
        Serialize(Calibration());
        image[offset] ^= 0x01;

        EXPECT_FALSE(Deserialize().has_value()) << offset;
    }
}

TEST_F(NvmRecordTest, a_configuration_record_is_not_read_as_a_calibration_record)
{
    std::array<uint8_t, services::ConfigRecord::size> config{};
    services::ConfigRecord::Serialize(services::ConfigData{}, infra::MakeRange(config));

    std::copy(config.begin(), config.begin() + image.size(), image.begin());

    EXPECT_FALSE(Deserialize().has_value());
}

TEST(CurrentNoise, a_noise_above_255_milliampere_is_stored_instead_of_saturating)
{
    EXPECT_EQ(300, services::CurrentNoiseMilliAmpere(0.3f));
    EXPECT_NEAR(0.3f, services::CurrentNoiseAmpere(300), 1e-6f);
}
